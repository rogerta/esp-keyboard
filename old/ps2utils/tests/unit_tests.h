// Simple unit testing for arduino projects.

#include <assert.h>
#include <iostream>


namespace testing {

// Operators //////////////////////////////////////////////////////////////////

template <typename E, typename A>
class BinaryCmpOp {
 public:
  BinaryCmpOp(const E& e, const A& a) : e_(e), a_(a) {}
  virtual bool operator()() const = 0;
  const E& e() const { return e_; }
  const A& a() const { return a_; }
 private:
  const E& e_;
  const A& a_;
};

template <typename E, typename A>
class EqOp : public BinaryCmpOp<E,A> {
 public:
  EqOp(const E& e, const A& a) : BinaryCmpOp<E,A>(e, a) {}
  virtual bool operator()() const { return this->e() == this->a(); }
};

template <typename E, typename A>
class NeOp : public BinaryCmpOp<E,A> {
 public:
  NeOp(const E& e, const A& a) : BinaryCmpOp<E,A>(e, a) {}
  virtual bool operator()() const { return this->e() != this->a(); }
};

template <typename V1, typename V2>
class LtOp : public BinaryCmpOp<V1,V2> {
 public:
  LtOp(const V1& v1, const V2& v2) : BinaryCmpOp<V1,V2>(v1, v2) {}
  virtual bool operator()() const { return this->e() < this->a(); }
};

template <typename V1, typename V2>
class GtOp : public BinaryCmpOp<V1,V2> {
 public:
  GtOp(const V1& v1, const V2& v2) : BinaryCmpOp<V1,V2>(v1, v2) {}
  virtual bool operator()() const { return this->e() > this->a(); }
};

template <typename E, typename A>
EqOp<E,A> MakeEqOp(const E& e, const A& a) {
  return EqOp<E,A>(e, a);
}

template <typename E, typename A>
NeOp<E,A> MakeNeOp(const E& e, const A& a) {
  return NeOp<E,A>(e, a);
}

template <typename V1, typename V2>
LtOp<V1,V2> MakeLtOp(const V1& v1, const V2& v2) {
  return LtOp<V1,V2>(v1, v2);
}

template <typename V1, typename V2>
GtOp<V1,V2> MakeGtOp(const V1& v1, const V2& v2) {
  return GtOp<V1,V2>(v1, v2);
}


// TestCase ///////////////////////////////////////////////////////////////////

class TestCase {
 public:
  typedef TestCase* (*Factory)();
  virtual ~TestCase();
  const char* name() const { return name_; }
  bool has_error() const { return has_error_; }
  void set_has_error() { has_error_ = true; }
  virtual void SetUp() {}
  virtual void TearDown() {}
  virtual void Run() = 0;

 protected:
  TestCase();
  void set_name(const char* name);

  template <typename E, typename A>
  void expectCondition(const char* es, const char* as,
                       const char* filename, int lineno,
                       const BinaryCmpOp<E,A>& op);

 private:
  const char* name_;
  bool has_error_;
};


// Template function declarations /////////////////////////////////////////////

template <typename E, typename A>
void TestCase::expectCondition(const char* es,
                               const char* as,
                               const char* filename,
                               int lineno,
                               const BinaryCmpOp<E, A>& op) {
  if(op())
    return;

  has_error_ = true;

  if (!es) {
    std::cout << std::endl
              << "*** FAILURE at " << filename << ":" << lineno << std::endl
              << "Expected: " << as << " to be " << (op.e() ? "true" : "false")
              << std::endl
              << std::endl;
  } else {
    std::cout << std::endl
              << "*** FAILURE at " << filename << ":" << lineno << std::endl
              << "Expected: '" << es << "' with value: " << op.e() << std::endl
              << "     Got: '" << as << "' with value: " << op.a() << std::endl
              << std::endl;
  }
}


// TestCaseFactoryInstaller ///////////////////////////////////////////////////

class TestCaseFactoryInstaller {
 public:
  TestCaseFactoryInstaller(TestCase::Factory factory);
};

#define __TEST_IMPL(case_name, parent_class, name)                        \
class name##_##case_name##_Test : public parent_class {                   \
 public:                                                                  \
  name##_##case_name##_Test() {set_name(#case_name "." #name);}           \
  virtual void Run();                                                     \
  static testing::TestCase* Factory() {                                   \
    return new name##_##case_name##_Test();                               \
  }                                                                       \
} g_##name##_##case_name##_Test;                                          \
static testing::TestCaseFactoryInstaller name##_##case_name##_Installer(  \
    name##_##case_name##_Test::Factory);                                  \
void name##_##case_name##_Test::Run()

#define TEST(name) __TEST_IMPL(Test, testing::TestCase, name)
#define TEST_F(parent_class, name) \
    __TEST_IMPL(parent_class, parent_class, name)

#define EXPECT_EQ(e,a) \
    expectCondition(#e, #a, __FILE__, __LINE__, testing::MakeEqOp(e, a))
#define EXPECT_NE(e,a) \
    expectCondition(#e, #a, __FILE__, __LINE__, testing::MakeNeOp(e, a))

#define EXPECT_LT(v1,v2) \
    expectCondition(#v1, #v2, __FILE__, __LINE__, testing::MakeLtOp(v1, v2))

#define EXPECT_GT(v1,v2) \
    expectCondition(#v1, #v2, __FILE__, __LINE__, testing::MakeGtOp(v1, v2))

#define EXPECT_TRUE(a) \
    expectCondition(0, #a, __FILE__, __LINE__, testing::MakeEqOp(true, a))
#define EXPECT_FALSE(a) \
    expectCondition(0, #a, __FILE__, __LINE__, testing::MakeEqOp(false, a))

}  // namespace testing
