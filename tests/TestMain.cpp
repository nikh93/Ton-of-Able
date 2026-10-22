#include <juce_core/juce_core.h>
#include <iostream>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int index = 0; index < runner.getNumResults(); ++index)
        if (const auto* result = runner.getResult (index); result != nullptr && result->failures != 0)
        {
            failures += result->failures;
            std::cerr << result->unitTestName << ": " << result->subcategoryName << " failed\n";
        }
    std::cout << runner.getNumResults() << " test sections, " << failures << " failures\n";

    return failures == 0 ? 0 : 1;
}
