#ifndef FWLEDMATRIX_HPP_FWLEDMATRIX_HPP
#define FWLEDMATRIX_HPP_FWLEDMATRIX_HPP

#include <fstream>
#include <utility>
#include <vector>
#include <cstdint>
#include <memory>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <expected>
#include <optional>

/**
 * @internal
 * Internal macro to return error automatically when writing data to serial port
 */
#define FWLEDMATRIX_GUARDED_WRITE(...) if (const auto error {serial_write(__VA_ARGS__)}) { return *error; }

/**
 * A namespace with various helpful concepts to determine the validity of some types
 */
namespace fwledconcepts {
    /**
     *  @tparam Ts the types to check.
     *
     *  Concept to verify that all parameters to fwledmatrix::serial_write are valid.
     *  This means that either they are a vector of or are a std::uint8_t.
     */
    template <typename... Ts>
    concept is_writable_on_serial = requires(Ts... values) {
        {
            std::conditional_t<((std::is_same_v<decltype(values), std::uint8_t> ||
                                 std::is_same_v<decltype(values), std::vector<std::uint8_t>>) && ...),
                                 std::true_type,
                                std::false_type>
            {}

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
    static constexpr std::uint8_t MAGIC_BYTES[]{0x32, 0xAC};

    /**
     * @internal
     * The magic byte to adjust brightness.
     */
    static constexpr std::uint8_t BRIGHTNESS_BYTE{0x00};

    /**
     * @internal
     * The magic byte to stage a column.
     */
    static constexpr std::uint8_t STAGE_BYTE{0x07};

    /**
     * @internal
     * The magic byte to commit a column.
     */
    static constexpr std::uint8_t COMMIT_BYTE{0x08};

    /**
     * @internal
     * The magic byte to send a pattern.
     */
    static constexpr std::uint8_t PATTERN_BYTE{0x01};

    /**
     * @internal
     * The magic byte to do the percentage pattern
     */
    static constexpr std::uint8_t PERCENTAGE_PATTERN_BYTE{0x00};

    /**
     * @internal
     * The magic byte to animate the current pattern
     */
    static constexpr std::uint8_t ANIMATE_PATTERN_BYTE{0x04};

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
     * @returns an optional with an error message if a serial port write fails.
     *
     * @internal
     * Internal function to write a message to the serial port. Assumes the serial port is in working order and open.
     */
    template <typename... Params> requires fwledconcepts::is_writable_on_serial<Params...>
    [[nodiscard]] std::optional<std::string> serial_write(const std::uint8_t command, const Params&... params) const {
        std::vector serial_payload{MAGIC_BYTES[0], MAGIC_BYTES[1], command};

        (vector_add(serial_payload, params), ...);

        long bytes_written{0};

        while (bytes_written < static_cast<long>(serial_payload.size())) {
            const long this_write_written_bytes{
                write(m_serial_conn, &serial_payload[0], serial_payload.size())
            };

            if (this_write_written_bytes == -1) {
                return "fwledmatrix: failed to write to serial port";
            }

            bytes_written += this_write_written_bytes;
        }

        return std::nullopt;
    }

    /**
     * @internal
     * @tparam T The return type. This can be any unsigned integer.
     *
     * @returns the data read from the serial port packed into some unsigned integer or an error if the
     *          data could not be read.
     *
     * Internal function to read data from the serial port and pack it into an unsinged integer.
     * Assumes the serial port is in working order and open.
     */
    template <typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T>
    [[nodiscard]] std::expected<T, std::string> serial_read() const {
        std::ptrdiff_t result{};
        read(m_serial_conn, &result, sizeof(T));

        if (result == -1) {
            return std::unexpected{"fwledmatrix: failed to read from serial port"};
        }

        return static_cast<T>(result);
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
     * List of potential patterns in an enum. We skip percentage on purpose as we will provide another function for
     * it because it requires an extra parameter.
     */
    enum class pattern : std::uint8_t {
        GRADIENT = 0x01,
        DOUBLE_GRADIENT,
        DISPLAY_LOTUS_HORIZONTAL,
        ZIGZAG,
        FULL_BRIGHTNESS,
        DISPLAY_PANIC,
        DISPLAY_LOTUS_VERTICAL
    };

    /**
     * @throws std::runtime_error when the serial port cannot be connected to
     *
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
        m_serial_conn{-1}

    {
        if (const auto error {serial_connect()}) {
            throw std::runtime_error{*error};
        }

        brightness(255);
        flush_matrix();
        animate_pattern(false);
    }

    /**
     * @throws std::runtime_error if failed to connect to serial as it is closed in the destructor
     *
     * Copy constructor connects to serial also
     */
    fwledmatrix(const fwledmatrix& other) :
        m_serial_port{other.m_serial_port},
        m_current_state{other.m_current_state},
        m_next_state{other.m_next_state},
        m_current_brightness{other.m_current_brightness},
        m_next_brightness{other.m_next_brightness},
        m_serial_conn{-1}
    {

        if (const auto error{serial_connect()}) {
            throw std::runtime_error{*error};
        }
    }

    /**
     * Closes the serial connection on destruction of the object if the serial is connected.
     * Hopefully there is no error but the user may want to check.
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
     * @returns an optional with an error message when serial port cannot be connected to.
     *
     * Function to connect to the serial port of the LED matrix.
     */
    std::optional<std::string> serial_connect() {
        if (connected()) {
            return std::nullopt;
        }

        m_serial_conn = open(m_serial_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);

        // Setup serial port
        if (m_serial_conn == -1) {
            return "fwledmatrix: failed to open serial port";
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
        serial_settings.c_cflag &= ~OPOST; // No output processing

        if (tcsetattr(m_serial_conn, TCSANOW, &serial_settings) != 0) {
            return "fwledmatrix: failed to set serial port settings";
        }

        return std::nullopt;
    }

    /**
     * @returns an optional value that may have an error message as a string.
     *
     * Disconnect from the LED matrix's serial port if not disconnected already.
     */
    std::optional<std::string> disconnect() {
        if (!connected()) {
            return std::nullopt;
        }

        if (close(m_serial_conn) == -1) {
            return "fwledmatrix: serial failed to close";
        }

        m_serial_conn = -1;

        return std::nullopt;
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
     * @returns an optional that may have an error message if we are unable to flush to the LED matrix.
     *
     * Flushes the changes made onto the LED matrix with the fwledmatrix::get, fwledmatrix::set,
     * fwledmatrix::operator[], fwledmatrix::clear, and fwledmatrix::brightness functions.
     */
    std::optional<std::string> flush_matrix() {
        if (!connected()) {
            return "fwledmatrix: serial port is not connected";
        }

        // Update brightness
        if (m_current_brightness != m_next_brightness) {
            FWLEDMATRIX_GUARDED_WRITE(BRIGHTNESS_BYTE, m_next_brightness);
            m_current_brightness = m_next_brightness;
        }

        // Send columns
        for (std::uint8_t col{0}; col < width; ++col) {
            if (m_current_state[col] != m_next_state[col]) {
                FWLEDMATRIX_GUARDED_WRITE(STAGE_BYTE, col, m_next_state[col]);
            }
        }

        // Render changes on display and update buffer
        FWLEDMATRIX_GUARDED_WRITE(COMMIT_BYTE);
        m_current_state = m_next_state;

        return std::nullopt;
    }

    /**
     * @param p the pattern to display.
     *
     * @returns an optional with may contain an error message if we are unable to flush the LED matrix.
     *
     * Sets and immediately flushes a pattern to the LED matrix.
     */
    std::optional<std::string> led_pattern(const pattern p) const {
        if (!connected()) {
            return "fwledmatrix: serial port is not connected";
        }

        FWLEDMATRIX_GUARDED_WRITE(PATTERN_BYTE, static_cast<std::uint8_t>(p));

        return std::nullopt;
    }

    /**
     * @param animate animate the current pattern or not.
     *
     * @returns an optional which may contain an error message if we are unable to flush the LED matrix.
     *
     * Sets whether to animate the current pattern or not.
     */
    std::optional<std::string> animate_pattern(const bool animate) const {
        if (!connected()) {
            return "fwledmatrix: serial port is not connected";
        }

        FWLEDMATRIX_GUARDED_WRITE(ANIMATE_PATTERN_BYTE, static_cast<std::uint8_t>(animate));

        return std::nullopt;
    }

    /**
     * @returns an optional which may contain an error message if we are unable to flush the LED matrix.
     *
     * Sets the percentage fill of the LED matrix and immediately flushes
     */
    std::optional<std::string> percentage(const std::uint8_t percentage) const {
        if (percentage > 100) {
            return "fwledmatrix: the requested percentage is greater than 100";
        }

        FWLEDMATRIX_GUARDED_WRITE(PATTERN_BYTE, PERCENTAGE_PATTERN_BYTE, percentage);

        return std::nullopt;
    }
};

#endif //FWLEDMATRIX_HPP_FWLEDMATRIX_HPP
