#ifndef FWLEDMATRIX_HPP_FWLEDMATRIX_HPP
#define FWLEDMATRIX_HPP_FWLEDMATRIX_HPP

#include <fstream>
#include <utility>
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>
#include <cassert>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>

/**
 * A namespace with various helpful concepts to determine the validity of some types
 */
namespace fwledconcepts {
    /**
     *  @tparam Ts the type to check.
     *
     *  Concept to verify that all parameters to fwledmatrix::serial_write are valid.
     *  This means that either they are a vector of or are a std::uint8_t.
     */
    template <typename... Ts>
    concept is_writable_on_serial = requires(Ts... values) {
        {
            std::conditional_t<((std::is_same_v<decltype(values), std::uint8_t> || std::is_same_v<decltype(values), std::vector<std::uint8_t>>) && ...), std::true_type, std::false_type>{}
        } -> std::same_as<std::true_type>;
    };
}


/**
 * A class representing a single Framework LED Matrix Input Module.
 * There is no bounds checking for LED coordinates so be careful.
 */
class fwledmatrix {
    /**
     * @internal
     * The magic bytes to open command mode for the LED matrix
     */
    static constexpr std::uint8_t magic_bytes[]{0x32, 0xAC};

    /**
     * @internal
     * The magic byte to adjust brightness
     */
    static constexpr std::uint8_t brightness_byte{0x00};

    /**
     * @internal
     * The magic byte to stage a column
     */
    static constexpr std::uint8_t stage_byte{0x07};

    /**
     * @internal
     * The magic byte to commit a column
     */
    static constexpr std::uint8_t commit_byte{0x08};

    /**
     * @internal
     * Internal serial port path.
     */
    std::string m_serial_port;

    /**
     * @internal
     * Internal vector representing current state of matrix. Updated when .flush is called.
     * Represented as a vector of height 9 and width of 34 to make it easy to compare columns and flush them.
     * Uses std::uint8_t because std::vector<bool> has some pitfalls.
     */
    std::vector<std::vector<std::uint8_t>> m_current_state;

    /**
     * @internal
     * Internal vector representing next state of matrix. Updated by calling various public methods of this class.
     * Represented as a vector of height 9 and width of 34 to make it easy to compare columns and flush them.
     * Uses std::uint8_t because std::vector<bool> has some pitfalls.
     */
    std::vector<std::vector<std::uint8_t>> m_next_state;

    /**
     * @internal
     * Internal unsigned character representing the current brightness of the entire matrix.
     */
    std::uint8_t m_current_brightness;

    /**
     * @internal
     * Internal unsigned character representing the next brightness of the entire matrix.
     */
    std::uint8_t m_next_brightness;

    /**
     * @internal
     * Internal int representing the file descriptor for the serial port.
     */
    int m_serial_conn;

    /**
     * @internal
     * Internal helper function to insert items of different types in a vector.
     */
    template <typename T, typename U>
    static void vector_add(std::vector<T>& vector, const U& item) {
        if constexpr (std::is_same_v<std::vector<T>, U>) {
            vector.append_range(item);
        } else {
            vector.push_back(item);
        }
    }

    /**
     * @tparam Params the various types that are being written to the serial port of the LED matrix.
     *
     * @internal
     * Internal function to write a message to the serial port. Assumes the serial port is in working order and open.
     */
    template <typename... Params> requires fwledconcepts::is_writable_on_serial<Params...>
    void serial_write(const std::uint8_t command, const Params&... params) const {
        std::vector serial_payload{magic_bytes[0], magic_bytes[1], command};

        (vector_add(serial_payload, params), ...);

        std::size_t bytes_written{0};

        while (bytes_written < serial_payload.size()) {
            bytes_written += write(m_serial_conn, &serial_payload[0], serial_payload.size());
        }
    }

    /**
     * @internal
     * @tparam T The return type. This can be any unsigned integer.
     * @returns the data read from the serial port packed into some unsigned integer.
     *
     * Internal function to read data from the serial port and pack it into an unsinged integer.
     * Assumes the serial port is in working order and open.
     */
    template <typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T>
    T serial_read() const {
        T result{};
        return read(m_serial_conn, &result, sizeof(T));
    }

public:
    /**
     * The width in leds of the matrix.
     */
    static constexpr int width{9};

    /**
     * The height in leds of the matrix.
     */
    static constexpr int height{34};

    /**
     * @param serial_port the path to the serial port to open and write control data to.
     * Most likely `/dev/ttyACM0` or `/dev/ttyACM1`.
     *
     * Initializes the class and clears the matrix so it is blank.
     */
    explicit fwledmatrix(std::string serial_port) :
        m_serial_port{std::move(serial_port)},
        m_current_state(width, std::vector<uint8_t>(height, 0)),
        m_next_state{m_current_state},
        m_current_brightness{0},
        m_next_brightness{0},
        m_serial_conn{-1} {
        serial_connect();
        brightness(255);
        flush();
    }

    /**
     * Closes the serial connection on destruction of the object if the serial is connected
     */
    ~fwledmatrix() {
        disconnect();
    }

    /**
     * @returns whether we are connected to the serial port of the LED matrix or not
     */
    [[nodiscard]] bool connected() const {
        return m_serial_conn != -1;
    }

    /**
     * @throws std::runtime_error when serial port cannot be connected to.
     *
     * Function to connect to the serial port of the LED matrix.
     */
    void serial_connect() {
        if (connected()) {
            return;
        }

        m_serial_conn = open(m_serial_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);

        // Setup serial port
        if (m_serial_conn == -1) {
            throw std::runtime_error{"fwledmatrix: failed to open serial port"};
        }

        struct termios serial_settings;
        tcgetattr(m_serial_conn, &serial_settings);

        // R/W @ baud rate of 115200
        cfsetspeed(&serial_settings, B115200);
        cfsetospeed(&serial_settings, B115200);

        serial_settings.c_cflag &= ~PARENB; // No parity
        serial_settings.c_cflag &= ~CSTOPB; // 2 stop bits
        serial_settings.c_cflag &= ~CSIZE; // Clear mask for data size
        serial_settings.c_cflag |= CS8; // Data bits = 8
        serial_settings.c_cflag &= ~CRTSCTS; // No hardware flow control
        serial_settings.c_cflag |= CREAD | CLOCAL; // Enable receiver and ignore modem control lines

        serial_settings.c_cflag &= ~(IXON | IXOFF | IXANY); // Disable XON/XOFF for input/output flow control
        serial_settings.c_cflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Non canonical mode
        serial_settings.c_cflag &= ~OPOST; // No ouput processing

        if (tcsetattr(m_serial_conn, TCSANOW, &serial_settings) != 0) {
            throw std::runtime_error{"fwledmatrix: failed to set serial port settings"};
        }
    }

    /**
     * Disconnect from the LED matrix's serial port if not disconnected already.
     */
    void disconnect() {
        if (!connected()) {
            return;
        }

        close(m_serial_conn);
        m_serial_conn = -1;
    }

    /**
     * @returns the string representation of the serial port.
     */
    [[nodiscard]] const std::string& serial_port() const {
        return m_serial_port;
    }

    /**
    * @param x the x coordinate of the LED.
    * @param y the y coordinate of the LED.
    *
    * @returns the LED's brightness.
    */
    [[nodiscard]] std::uint8_t get(const std::size_t x, const std::size_t y) const {
        return m_next_state[x][y];
    }

    /**
     * @param x the x coordinate of the LED to turn on.
     * @param y the y coordinate of the LED to turn on.
     * @param brightness the brightness of the LED [0-255]
     *
     * Function to set the brightness of a LED.
     */
    void set(const std::size_t x, const std::size_t y, const std::uint8_t brightness) {
        m_next_state[x][y] = brightness;
    }

    /**
     * Operator to get the row of a matrix in the next state.
     *
     * @returns a mutable reference to the column's vector, which you can assign LED brightnesses to.
     */
    std::vector<std::uint8_t>& operator[](const std::size_t x) {
        return m_next_state[x];
    }

    /**
     * Clears the next state.
     */
    void clear() {
        m_next_state = std::vector(width, std::vector<std::uint8_t>(height, 0));
    }

    /**
    * @returns the current brightness of the entire LED matrix
    */
    [[nodiscard]] std::uint8_t brightness() const {
        return m_current_brightness;
    }

    /**
     * @param brightness the brightness of the entire LED matrix.
     */
    void brightness(const std::uint8_t brightness) {
        m_next_brightness = brightness;
    }

    /**
     * @throws std::runtime_error if the serial port cannot be opened.
     * Flushes the changes made onto the LED matrix.
     */
    void flush() {
        // Update brightness
        if (m_current_brightness != m_next_brightness) {
            serial_write(brightness_byte, m_next_brightness);
            m_current_brightness = m_next_brightness;
        }

        // Send columns
        for (std::uint8_t col{0}; col < width; ++col) {
            if (m_current_state[col] != m_next_state[col]) {
                serial_write(stage_byte, col, m_next_state[col]);
            }
        }

        // Render changes on display and update buffer
        serial_write(commit_byte);
        m_current_state = m_next_state;
    }
};

#endif //FWLEDMATRIX_HPP_FWLEDMATRIX_HPP
