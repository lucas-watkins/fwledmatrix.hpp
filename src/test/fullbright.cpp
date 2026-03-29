#include <chrono>
#include <fwledmatrix.hpp>
#include <thread>

int main() {
    fwledmatrix matrix{"/dev/ttyACM0"};
    fwledmatrix matrix1{"/dev/ttyACM1"};

    for (int x{0}; x < fwledmatrix::width; ++x) {
        for (int y{0}; y < fwledmatrix::height; ++y) {
            matrix.set(x, y, 255);
            matrix1.set(x, y, 255);
        }
    }

    matrix.flush_matrix();
    matrix1.flush_matrix();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    matrix.clear();
    matrix1.clear();

    for (int x{0}; x < fwledmatrix::width; ++x) {
        for (int y{0}; y < fwledmatrix::height; ++y) {
            matrix.set(x, y, 0);
            matrix1.set(x, y, 0);
        }
    }

    matrix.flush_matrix();
    matrix1.flush_matrix();

    return 0;
}
