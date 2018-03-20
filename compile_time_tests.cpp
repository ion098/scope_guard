/*
 *  Created on: 27/02/2018
 *      Author: ricab
 */

#include "scope_guard.hpp"
#include <utility>
#include <functional>
using namespace sg;

/* --- first some test helpers --- */

////////////////////////////////////////////////////////////////////////////////
namespace
{
  constexpr auto EMSG = "message in a bottle";
  void non_throwing() noexcept { }
  [[noreturn]] void throwing() { throw std::runtime_error{EMSG}; }
  void meh() { }

  int returning() noexcept { return 42; }


  using StdFun = std::function<void()>;
  StdFun throwing_stdfun{throwing};
  StdFun non_throwing_stdfun{non_throwing}; // drops noexcept
  StdFun meh_stdfun{meh};

  std::function<int()> returning_stdfun{returning}; // drops noexcept


  auto throwing_lambda = []{ throwing(); };
  auto non_throwing_lambda = []() noexcept { non_throwing(); };
  auto meh_lambda = []{ meh(); };

  auto returning_lambda = []() noexcept { return returning(); };


  auto throwing_bound = std::bind(throwing);
  auto non_throwing_bound = std::bind(non_throwing); // drops noexcept
  auto meh_bound = std::bind(meh);

  auto returning_bound = std::bind(returning); // drops noexcept


  struct throwing_struct
  {
    [[noreturn]] void operator()() { throwing(); }
  } throwing_functor;
  struct non_throwing_struct
  {
    void operator()() const noexcept { non_throwing(); }
  } non_throwing_functor;
  struct meh_struct
  {
    void operator()() const { meh(); }
  } meh_functor;

  struct returning_struct
  {
    int operator()() const noexcept { return returning(); }
  } returning_functor;

  struct nocopy_nomove // non-copyable and non-movable
  {
    void operator()() const noexcept { non_throwing(); }

    nocopy_nomove() noexcept = default;
    ~nocopy_nomove() noexcept = default;

    nocopy_nomove(const nocopy_nomove&) = delete;
    nocopy_nomove& operator=(const nocopy_nomove&) = delete;
    nocopy_nomove(nocopy_nomove&&) = delete;
    nocopy_nomove& operator=(nocopy_nomove&&) = delete;
  };

  struct potentially_throwing_dtor
  {
    void operator()() const noexcept { non_throwing(); }

    ~potentially_throwing_dtor() noexcept(false) {}

    potentially_throwing_dtor() noexcept = default;
    potentially_throwing_dtor(const potentially_throwing_dtor&) noexcept
      = default;
    potentially_throwing_dtor(potentially_throwing_dtor&&) noexcept = default;
  };

  struct throwing_copy
  {
    void operator()() const noexcept { non_throwing(); }

    throwing_copy(const throwing_copy&) noexcept(false) {}

    ~throwing_copy() noexcept = default;
    throwing_copy() noexcept = default;
    throwing_copy(throwing_copy&&) noexcept = default;
  };

  struct throwing_move
  {
    void operator()() const noexcept { non_throwing(); }

    throwing_move(throwing_move&&) noexcept(false) {}

    ~throwing_move() noexcept = default;
    throwing_move() noexcept = default;
    throwing_move(const throwing_move&) noexcept = default;
  };

  struct nomove_throwing_copy
  {
    void operator()() const noexcept { non_throwing(); }

    nomove_throwing_copy(const nomove_throwing_copy&) noexcept(false) {}

    /*
     * nomove_throwing_copy(nomove_throwing_copy&&) noexcept;
     *
     * not declared! move ctor absent but not deleted - does not participate in
     * overload resolution, so copy ctor can still be selected
     */

    ~nomove_throwing_copy() noexcept = default;
    nomove_throwing_copy() noexcept = default;
  };

  struct nothrow
  {
    void operator()() const noexcept { non_throwing(); }

    ~nothrow() noexcept = default;
    nothrow() noexcept = default;
    nothrow(const nothrow&) noexcept = default;
    nothrow(nothrow&&) noexcept = default;
  };


  /* --- tests that always succeed --- */

#ifdef test_1
    static_assert(noexcept(make_scope_guard(std::declval<void(*)()noexcept>())),
                  "make_scope_guard not noexcept");
#endif

#ifdef test_2
    static_assert(noexcept(detail::scope_guard<void(*)()noexcept>{
      std::declval<void(*)()noexcept>()}), "scope_guard ctor not noexcept");
#endif

#ifdef test_3
    static_assert(noexcept(make_scope_guard(std::declval<void(*)()noexcept>())
                           .~scope_guard()),
                  "scope_guard dtor not noexcept");
#endif

    /**
     * Test nothrow character of make_scope_guard for different value categories
     * of a type with a throwing destructor
     */
    void test_throwing_dtor_throw_spec_good()
    {
#ifdef test_4
      potentially_throwing_dtor x;
      static_assert(noexcept(make_scope_guard(x)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue object whose dtor throws (should deduce "
                    "reference and avoid destruction entirely)");
#endif
#ifdef test_5
      potentially_throwing_dtor x;
      auto& r = x;
      static_assert(noexcept(make_scope_guard(r)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to an object whose dtor throws "
                    "(should deduce reference and avoid destruction entirely)");
#endif
#ifdef test_6
      potentially_throwing_dtor x;
      const auto& cr = x;
      static_assert(noexcept(make_scope_guard(cr)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to a const object whose dtor "
                    "throws (should deduce reference and avoid destruction "
                    "entirely)");
#endif
    }

    /**
     * Test nothrow character of make_scope_guard for different value categories
     * of a type with a throwing copy constructor
     */
    void test_throwing_copy_throw_spec()
    {
#ifdef test_7
      static_assert(noexcept(make_scope_guard(throwing_copy{})),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an rvalue object whose copy ctor throws");
#endif
#ifdef test_8
      throwing_copy x;
      static_assert(noexcept(make_scope_guard(x)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue object whose copy ctor throws (should "
                    "deduce reference and avoid copy entirely)");
#endif
#ifdef test_9
      throwing_copy x;
      static_assert(noexcept(make_scope_guard(std::move(x))),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an rvalue reference to an object whose copy ctor "
                    "throws");
#endif
#ifdef test_10
      throwing_copy x;
      auto& r = x;
      static_assert(noexcept(make_scope_guard(r)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to an object whose copy ctor "
                    "throws (should deduce reference and avoid copy entirely)");
#endif
#ifdef test_11
      throwing_copy x;
      const auto& cr = x;
      static_assert(noexcept(make_scope_guard(cr)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to a const object whose copy "
                    "ctor throws (should deduce reference and avoid copy "
                    "entirely)");
#endif
    }

    /**
     * Test nothrow character of make_scope_guard for different value categories
     * of a type with a throwing move constructor
     */
    void test_throwing_move_throw_spec()
    {
#ifdef test_12
      static_assert(!noexcept(make_scope_guard(throwing_move{})),
                    "make_scope_guard wrongly declared noexcept when instanced "
                    "with an rvalue object whose move ctor throws");
#endif
#ifdef test_13
      throwing_move x;
      static_assert(noexcept(make_scope_guard(x)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue object whose move ctor throws");
#endif
#ifdef test_14
      throwing_move x;
      static_assert(!noexcept(make_scope_guard(std::move(x))),
                    "make_scope_guard wrongly declared noexcept when instanced "
                    "with an rvalue reference to an object whose move ctor "
                    "throws");
#endif
#ifdef test_15
      throwing_move x;
      auto& r = x;
      static_assert(noexcept(make_scope_guard(r)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to an object whose move ctor "
                    "throws");
#endif
#ifdef test_16
      throwing_move x;
      const auto& cr = x;
      static_assert(noexcept(make_scope_guard(cr)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to a const object whose move "
                    "ctor throws");
#endif
    }

    /**
     * Test nothrow character of make_scope_guard for different value categories
     * of a type with a throwing copy constructor
     */
    void test_nomove_throwing_copy_throw_spec()
    {
#ifdef test_17
      static_assert(!noexcept(make_scope_guard(nomove_throwing_copy{})),
                    "make_scope_guard wrongly declared noexcept when instanced "
                    "with an rvalue object whose copy ctor throws and without "
                    "a move ctor");
#endif
#ifdef test_18
      nomove_throwing_copy x;
      static_assert(noexcept(make_scope_guard(x)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue object whose copy ctor throws and without "
                    "a move ctor (should deduce reference and avoid copy "
                    "entirely)");
#endif
#ifdef test_19
      nomove_throwing_copy x;
      static_assert(!noexcept(make_scope_guard(std::move(x))),
                    "make_scope_guard wrongly declared noexcept when instanced "
                    "with an rvalue reference to an object whose copy ctor "
                    "throws and without a move ctor");
#endif
#ifdef test_20
      nomove_throwing_copy x;
      auto& r = x;
      static_assert(noexcept(make_scope_guard(r)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to an object whose copy ctor "
                    "throws (should deduce reference and avoid copy entirely)");
#endif
#ifdef test_21
      nomove_throwing_copy x;
      const auto& cr = x;
      static_assert(noexcept(make_scope_guard(cr)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to a const object whose copy "
                    "ctor throws (should deduce reference and avoid copy "
                    "entirely)");
#endif
    }

    /**
     * Test nothrow character of make_scope_guard for different value categories
     * of a type with a non-throwing constructors and destructor
     */
    void test_nothrow_throw_spec()
    {
#ifdef test_22
      static_assert(noexcept(make_scope_guard(nothrow{})),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an rvalue object whose ctors and dtor do not throw");
#endif
#ifdef test_23
      nothrow x;
      static_assert(noexcept(make_scope_guard(x)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue object whose ctors and dtor do not throw");
#endif
#ifdef test_24
      nothrow x;
      static_assert(noexcept(make_scope_guard(std::move(x))),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an rvalue reference to an object whose ctors and "
                    "dtor do not throw");
#endif
#ifdef test_25
      nothrow x;
      auto& r = x;
      static_assert(noexcept(make_scope_guard(r)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to an object whose ctors and "
                    "dtor do not throw");
#endif
#ifdef test_26
      nothrow x;
      const auto& cr = x;
      static_assert(noexcept(make_scope_guard(cr)),
                    "make_scope_guard not declared noexcept when instanced "
                    "with an lvalue reference to a const object whose ctors "
                    "dtor do not throw");
#endif
    }

  /**
   * Test compilation successes with wrong usage of non-copyable and non-movable
   * objects to create scope_guards
   */
  void test_noncopyable_nonmovable_good()
  {
#ifdef test_27
    nocopy_nomove ncnm{};
    make_scope_guard(ncnm);
#endif
#ifdef test_28
    nocopy_nomove ncnm{};
    auto& ncnmr = ncnm;
    make_scope_guard(ncnmr);
#endif
#ifdef test_29
    nocopy_nomove ncnm{};
    const auto& ncnmcr = ncnm;
    make_scope_guard(ncnmcr);
#endif
  }

  /**
   * Test scope_guard can always be created with noexcept marked callables
   */
  void test_noexcept_good()
  {
#ifdef test_30
    make_scope_guard(non_throwing);
#endif
#ifdef test_31
    make_scope_guard(non_throwing_lambda);
#endif
#ifdef test_32
    make_scope_guard(non_throwing_functor);
#endif
  }


  /* --- tests that fail iff nothrow_invocable is required --- */

  /**
   * Highlight that scope_guard should not be created with throwing callables,
   * under penalty of an immediate std::terminate call. Test that compilation
   * fails on such an attempt when noexcept is required.
   */
  void test_noexcept_bad()
  {
#ifdef test_33
    make_scope_guard(throwing);
#endif
#ifdef test_34
    make_scope_guard(throwing_stdfun);
#endif
#ifdef test_35
    make_scope_guard(throwing_lambda);
#endif
#ifdef test_36
    make_scope_guard(throwing_bound);
#endif
#ifdef test_37
    make_scope_guard(throwing_functor);
#endif
  }

  /**
   * Highlight the importance of declaring scope_guard callables noexcept
   * (when they do not throw). Test that compilation fails on attempts to the
   * contrary when noexcept is required.
   */
  void test_noexcept_fixable()
  {
#ifdef test_38
    make_scope_guard(meh);
#endif
#ifdef test_39
    make_scope_guard(meh_stdfun);
#endif
#ifdef test_40
    make_scope_guard(meh_lambda);
#endif
#ifdef test_41
    make_scope_guard(meh_bound);
#endif
#ifdef test_42
    make_scope_guard(meh_functor);
#endif
  }

  /**
   * Highlight that some callables cannot be declared noexcept even if they are
   * known not to throw. Show that trying to create scope_guards with such
   * objects unfortunately (but unavoidably AFAIK) breaks compilation when
   * noexcept is required
   */
  void test_noexcept_unfortunate()
  {
#ifdef test_43
    make_scope_guard(non_throwing_stdfun);
#endif
#ifdef test_44
    make_scope_guard(non_throwing_bound);
#endif
  }


  /* --- tests that always fail --- */

  void test_throwing_dtor_throw_spec_bad()
  {
#ifdef test_45
    make_scope_guard(potentially_throwing_dtor{});
#endif
#ifdef test_46
    potentially_throwing_dtor x;
    make_scope_guard(std::move(x));
#endif
  }

  /**
   * Test that compilation fails when trying to copy-construct a scope_guard
   */
  void test_disallowed_copy_construction()
  {
    const auto guard1 = make_scope_guard(non_throwing);
#ifdef test_47
    const auto guard2 = guard1;
#endif
  }

  /**
   * Test that compilation fails when trying to copy-assign a scope_guard
   */
  void test_disallowed_copy_assignment()
  {
    const auto guard1 = make_scope_guard(non_throwing_lambda);
    auto guard2 = make_scope_guard(non_throwing_functor);
#ifdef test_48
    guard2 = guard1;
#endif
  }

  /**
   * Test that compilation fails when trying to move-assign a scope_guard
   */
  void test_disallowed_move_assignment()
  {
    auto guard = make_scope_guard(non_throwing);
#ifdef test_49
    guard = make_scope_guard(non_throwing_lambda);
#endif
  }

  /**
   * Test that compilation fails when trying to use a returning function to
   * create a scope_guard
   */
  void test_disallowed_return()
  {
#ifdef test_50
    make_scope_guard(returning);
#endif
#ifdef test_51
    make_scope_guard(returning_stdfun);
#endif
#ifdef test_52
    make_scope_guard(returning_lambda);
#endif
#ifdef test_53
    make_scope_guard(returning_bound);
#endif
#ifdef test_54
    make_scope_guard(returning_functor);
#endif
  }

  /**
   * Test compilation failures with wrong usage of non-copyable and non-movable
   * objects to create scope_guards
   */
  void test_noncopyable_nonmovable_bad()
  {
#ifdef test_55
    make_scope_guard(nocopy_nomove{});
#endif
#ifdef test_56
    nocopy_nomove ncnm{};
    make_scope_guard(std::move(ncnm));
#endif
  }
}

int main()
{
  test_throwing_dtor_throw_spec_good();
  test_throwing_copy_throw_spec();
  test_throwing_move_throw_spec();
  test_nomove_throwing_copy_throw_spec();
  test_nothrow_throw_spec();
  test_noncopyable_nonmovable_good();
  test_noexcept_good();

  test_noexcept_bad(); // this results in a call to std::terminate
  test_noexcept_fixable();
  test_noexcept_unfortunate();

  test_throwing_dtor_throw_spec_bad();
  test_disallowed_copy_construction();
  test_disallowed_copy_assignment();
  test_disallowed_move_assignment();
  test_disallowed_return();
  test_noncopyable_nonmovable_bad();

  return 0;
}
