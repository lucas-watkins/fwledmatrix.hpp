//
// Created by lucas on 3/29/26.
//

#include <fwledmatrix.hpp>
#include <vector>
#include <iostream>

#define GUARD_APPLY(matrices, func) for(auto&m:matrices) {if(const auto error {m.func}){std::cout <<*error<< '\n'; std::cout << strerror(errno) << '\n';}}

int main() {
    std::vector matrices {fwledmatrix{"/dev/ttyACM0"}, fwledmatrix{"/dev/ttyACM1"}};

    int choice{0};

    do {
        std::cout << "What pattern would you like to display?\n\n";
        std::cout << "1. Percentage\n";
        std::cout << "2. Gradient\n";
        std::cout << "3. DoubleGradient\n";
        std::cout << "4. DisplayLotusHorizontal\n";
        std::cout << "5. ZigZag\n";
        std::cout << "6. FullBrightness\n";
        std::cout << "7. DisplayPanic\n";
        std::cout << "8. DisplayLotusVertical\n\n";
        std::cout << "OR:\n";
        std::cout << "9. Exit\n\n";

        std::cout << "Pattern: ";
        std::cin >> choice;

        if (!std::cin) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

    } while (choice < 1 || choice > 9);

    if (choice == 9) {
        return 0;
    }

    if (choice == 1) {
        int percentage{-1};

        do {
            std::cout << "\nWhat percentage fill would you like the LED matrix to be?: ";
            std::cin >> percentage;

            if (!std::cin) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }

        } while (percentage < 0 || percentage > 100);

        GUARD_APPLY(matrices, percentage(percentage));

        return 0;
    }

    const auto pattern{static_cast<fwledmatrix::pattern>(choice - 1)};

    GUARD_APPLY(matrices, led_pattern(pattern));

    char animate{0};

    do {
        std::cout << "Would you like to animate the current pattern (y/n)?: ";
        std::cin >> animate;

        animate = static_cast<char>(std::tolower(animate));

        if (!std::cin) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

    } while (animate != 'y' && animate != 'n');

    if (animate == 'y') {
        GUARD_APPLY(matrices, animate_pattern(true));
    } else {
        GUARD_APPLY(matrices, animate_pattern(false));
    }

    return 0;
}
