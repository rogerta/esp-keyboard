// build with:
// gcc -I. -c *.c *.cpp
// ld *.o -maxosx_version_min 10.8 -lSystem -o unit_tests

#include <math.h>
#include <iomanip>
#include <vector>

#include "unit_tests.h"


static std::vector<testing::TestCase::Factory> gTestFacroties;

// TestCase ///////////////////////////////////////////////////////////////////

namespace testing {

TestCase::TestCase() : name_(0), has_error_(false) {}

TestCase::~TestCase() {}

void TestCase::set_name(const char* name) {
  name_ = name;
}


// TestCaseFactoryInstaller ///////////////////////////////////////////////////

TestCaseFactoryInstaller::TestCaseFactoryInstaller(TestCase::Factory factory) {
  gTestFacroties.push_back(factory);
}

}  // namespace testing

static void PrintOutput(int width,
                        int i,
                        int count,
                        bool is_success,
                        const char* tag,
                        const char* name) {
  const char* open_bracket = is_success ? "\033[1;32m[ " : "\033[1;31m[ ";

  std::cout << "(" << std::setw(width) << (i + 1) << "/" << count << ") "
            << open_bracket << tag << " ]\033[0m "
            << name << std::endl;
}

int main(int argc, char* argv[]) {
  std::cout << "START TESTS" << std::endl << std::endl;

  size_t ran = 0;
  size_t successful = 0;
  size_t failed = 0;

  std::vector<std::string> failed_tests;

  size_t count = gTestFacroties.size();
  int width = (int)log10(count) + 1;
  for (size_t i = 0; i < count; ++i) {
    testing::TestCase* test = gTestFacroties[i]();
    PrintOutput(width, i, count, true, "RUN   ", test->name());
    try {
      ++ran;
      test->SetUp();
      if (!test->has_error())
        test->Run();
    } catch(...) {
      test->set_has_error();
    }
    test->TearDown();
    if (test->has_error()) {
      ++failed;
      failed_tests.push_back(test->name());
      PrintOutput(width, i, count, false, "FAILED", test->name());
    } else {
      ++successful;
      PrintOutput(width, i, count, true, "    OK", test->name());
    }
    delete test;
  }

  std::cout << std::endl << "FAILED    = "
            << std::setw(width) << failed << " / " << ran;
  std::cout << std::endl << "SUCCEEDED = "
            << std::setw(width) << successful << " / " << ran;

  if (failed_tests.size() > 0) {
    std::cout << std::endl << std::endl << "Failing tests:";
    for (auto i = failed_tests.begin(); i != failed_tests.end(); ++i)
      std::cout << std::endl << "   " << *i;
  } else {
    std::cout << std::endl << std::endl << "ALL TESTS COMPLETED SUCCESSFULLY";
  }

  std::cout << std::endl << std::endl << "END TESTS" << std::endl;
  return 0;
}

