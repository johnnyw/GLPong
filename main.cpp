#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <cassert>

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "GL/glew.h"
#include "GLFW/glfw3.h"

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

const float BALL_SIZE = 15.0f;
const float MAX_BALL_SPEED = 200.0f; // Pixels per sec

const float PLAYER_WIDTH = 15.0f;
const float PLAYER_HEIGHT = 100.0f;
const float PLAYER_SPEED = 200.0f;

const float HALF_PLAYER_HEIGHT = PLAYER_HEIGHT / 2.0f;

int randomInteger(int min, int max) {
    static bool initialized = false;
    if (!initialized) {
        srand(static_cast<unsigned int>(time(NULL)));
        initialized = true;
    }

    int diff = max - min;
    return (rand() % diff) + min;
}

float fclamp(float value, float min, float max) {
    assert(min < max);
    return fmin(min, fmax(value, max));
}

#ifndef NDEBUG
static char* createTimestampString() {
    time_t rawtime;
    time(&rawtime);
    
    tm *timeinfo = localtime(&rawtime);

    char *output = new char[80];
    strftime(output, 80, "[%Y-%m-%d %H:%M:%S] ", timeinfo);
    return output;
}

void debugPrint(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int length = vsnprintf(NULL, 0, format, args);

    if (length < 0) {
        return;
    }

    char *buffer = (char*)malloc(length + 1);
    vsnprintf(buffer, length + 1, format, args);
    va_end(args);

    char *timestamp = createTimestampString();

#ifdef _WIN32
    int wlength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, -1, NULL, 0);
    LPWSTR wbuffer = (LPWSTR)calloc(wlength, sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, -1, wbuffer, wlength);

    int timestampWideCharLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, timestamp, -1, NULL, 0);
    LPWSTR wTimestamp = (LPWSTR)calloc(timestampWideCharLength, sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, timestamp, -1, wTimestamp, timestampWideCharLength);

    CRITICAL_SECTION criticalSection;
    InitializeCriticalSection(&criticalSection);

    EnterCriticalSection(&criticalSection);
    OutputDebugString(wTimestamp);
    OutputDebugString(wbuffer);
    OutputDebugString(L"\n");
    LeaveCriticalSection(&criticalSection);

    DeleteCriticalSection(&criticalSection);

    free(wTimestamp);
    free(wbuffer);
#else
    printf("%s%s\n", timestamp, buffer);
#endif

    free(timestamp);
    free(buffer);
}
#else
#define debugPrint(format, ...)
#endif

struct Point {
    float x, y;
    
    Point() {
        x = y = 0;
    }

    Point(float _x, float _y) : x(_x), y(_y) {}
};

struct Rect {
    float x, y;
    float width, height;

    Rect(float _x, float _y, float _width, float _height) : x(_x), y(_y), width(_width), height(_height) {}

    Rect() {
        x = y = width = height = 0;
    }

    void cornerPoints(Point &topLeft, Point &topRight, Point &bottomLeft, Point &bottomRight) const {
        topLeft.x = x;
        topLeft.y = y;

        topRight.x = x + width;
        topRight.y = y;

        bottomLeft.x = x;
        bottomLeft.y = y + height;

        bottomRight.x = x + width;
        bottomRight.y = y + height;
    }

    bool intersects(const Rect &other) const {
        Point topLeft, topRight, bottomLeft, bottomRight;

        // Check if points of self are contained within other
        cornerPoints(topLeft, topRight, bottomLeft, bottomRight);

        if (other.contains(topLeft) || other.contains(topRight) || other.contains(bottomLeft) || other.contains(bottomRight)) {
            return true;
        }

        // Check if points of other are contained within self
        other.cornerPoints(topLeft, topRight, bottomLeft, bottomRight);

        if (contains(topLeft) || contains(topRight) || contains(bottomLeft) || contains(bottomRight)) {
            return true;
        }

        return false;
    }

    bool contains(const Point &point) const {
        if (point.x > x && point.x < x + width) {
            if (point.y > y && point.y < y + height) {
                return true;
            }
        }

        return false;
    }
};

struct Color {
    float r, g, b;
    Color(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}
};

struct Shader {
    Shader(GLenum type) : _type(type) {}

    bool compile(const std::string &source) {
        if (_type != GL_VERTEX_SHADER && _type != GL_FRAGMENT_SHADER) {
            debugPrint("Invalid argument for shader type.");
            return false;
        }
        
        _id = glCreateShader(_type);
        const char *cstr = source.c_str();
        glShaderSource(_id, 1, &cstr, NULL);

        glCompileShader(_id);

        GLint status;
        glGetShaderiv(_id, GL_COMPILE_STATUS, &status);

        if (status != GL_TRUE) {
            return false;
        }

        return true;
    }

    std::string getCompileStatus() {
        GLint infoLogLength;
        glGetShaderiv(_id, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength) {
            char *infoLog = new char[infoLogLength];
            glGetShaderInfoLog(_id, infoLogLength, NULL, infoLog);
            std::string result(infoLog);
            delete infoLog;
            return result;
        }

        return "";
    }

    GLuint getGLID() {
        return _id;
    }

private:
    GLuint _id;
    GLenum _type;
};

struct Program {
    Program(const Shader &vertexShader, const Shader &fragmentShader) : _vertexShader(vertexShader), _fragmentShader(fragmentShader) {}

    bool link() {
        _id = glCreateProgram();
        glAttachShader(_id, _vertexShader.getGLID());
        glAttachShader(_id, _fragmentShader.getGLID());

        glLinkProgram(_id);

        GLint status;
        glGetProgramiv(_id, GL_LINK_STATUS, &status);

        if (status != GL_TRUE) {
            return false;
        }

        return true;
    }

    std::string getLinkStatus() {
        GLint infoLogLength;
        glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength) {
            char *infoLog = new char[infoLogLength];
            glGetShaderInfoLog(_id, infoLogLength, NULL, infoLog);
            std::string result(infoLog);
            delete infoLog;
            return result;
        }

        return "";
    }

    GLuint getGLID() {
        return _id;
    }

private:
    GLuint _id;
    Shader _vertexShader;
    Shader _fragmentShader;
};

enum PlayerSide {
    PLAYER_LEFT,
    PLAYER_RIGHT
};

struct Player {
    Player(PlayerSide side) : _side(side) {
        _score = 0;
        _verticalSpeed = 0.0f;
        _coords = Rect(0, SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2, PLAYER_WIDTH, PLAYER_HEIGHT);

        switch (side) {
        case PLAYER_LEFT:
            _coords.x = 10;
            break;
        case PLAYER_RIGHT:
            _coords.x = SCREEN_WIDTH - PLAYER_WIDTH - 10;
            break;
        }
    }

    int getScore() {
        return _score;
    }

    void incrementScore() {
        ++_score;
    }

    void update(float elapsed) {
        moveVertical(_verticalSpeed * elapsed);
    }

    void setVerticalSpeed(float verticalSpeed) {
        _verticalSpeed = verticalSpeed;
    }

    Rect getCoords() const {
        return _coords;
    }

private:
    PlayerSide _side;
    int _score;
    Rect _coords;
    float _verticalSpeed;

    void moveVertical(float distance) {
        _coords.y += distance;
        if (_coords.y > SCREEN_HEIGHT - PLAYER_HEIGHT) {
            _coords.y = SCREEN_HEIGHT - PLAYER_HEIGHT;
        }

        if (_coords.y < 0) {
            _coords.y = 0;
        }
    }
};

bool shouldUpdateTitle = true;

struct Ball {
    static Ball& getInstance() {
        static Ball instance;
        return instance;
    }

    void update(float elapsed) {
        _x += _xSpeed * elapsed;
        _y += _ySpeed * elapsed;

        if (_x < 0) {
            _rightPlayer->incrementScore();
            reset();
            shouldUpdateTitle = true;
        }

        if (_x > SCREEN_WIDTH - BALL_SIZE) {
            _leftPlayer->incrementScore();
            reset();
            shouldUpdateTitle = true;
        }

        if (_y < 0 || _y > SCREEN_HEIGHT - BALL_SIZE) {
            _ySpeed = -_ySpeed;
        }

        const Rect ballRect(_x, _y, BALL_SIZE, BALL_SIZE);

        const Rect leftPlayerCoords = _leftPlayer->getCoords();
        if (leftPlayerCoords.intersects(ballRect)) {
            bounce(*_leftPlayer, _xSpeed, _ySpeed);
        }

        const Rect rightPlayerCoords = _rightPlayer->getCoords();
        if (rightPlayerCoords.intersects(ballRect)) {
            bounce(*_rightPlayer, _xSpeed, _ySpeed);
            _xSpeed = -_xSpeed;
        }
    }

    void getCoordinates(float &x, float &y) {
        x = _x;
        y = _y;
    }

    void getSpeed(float &xSpeed, float &ySpeed) {
        xSpeed = _xSpeed;
        ySpeed = _ySpeed;
    }

    void reset() {
        _x = (SCREEN_WIDTH / 2) - (BALL_SIZE / 2);
        _y = (SCREEN_HEIGHT / 2) - (BALL_SIZE / 2);

        int direction = randomInteger(1, 7);

        switch (direction) {
        case 1:
            // Up/right
            _xSpeed = MAX_BALL_SPEED;
            _ySpeed = -MAX_BALL_SPEED;
            break;
        case 2:
            // Right
            _xSpeed = MAX_BALL_SPEED;
            _ySpeed = 0.0f;
            break;
        case 3:
            // Down/right
            _xSpeed = MAX_BALL_SPEED;
            _ySpeed = MAX_BALL_SPEED;
            break;
        case 4:
            // Down/left
            _xSpeed = -MAX_BALL_SPEED;
            _ySpeed = MAX_BALL_SPEED;
            break;
        case 5:
            // Left
            _xSpeed = -MAX_BALL_SPEED;
            _ySpeed = 0.0f;
            break;
        case 6:
            // Up/left
            _xSpeed = -MAX_BALL_SPEED;
            _ySpeed = -MAX_BALL_SPEED;
            break;
        }
    }

    void setSpeed(float xSpeed, float ySpeed) {
        _xSpeed = xSpeed;
        _ySpeed = ySpeed;
    }

    void setLeftPlayer(Player *player) {
        _leftPlayer = player;
    }

    void setRightPlayer(Player *player) {
        _rightPlayer = player;
    }

private:
    float _x, _y;
    float _xSpeed, _ySpeed;

    Player *_leftPlayer;
    Player *_rightPlayer;

    Ball() {
        reset();
    }

    Ball(Ball const&) = delete;
    void operator=(Ball const&) = delete;

    void bounce(const Player &player, float &newXSpeed, float &newYSpeed) {
        const float ballCenterY = _y + (BALL_SIZE / 2);
        const float playerCenterY = player.getCoords().y + HALF_PLAYER_HEIGHT;
        const float normalizedDistance = fmin(1.0f, fabs(playerCenterY - ballCenterY) / HALF_PLAYER_HEIGHT);
        const float speedScaleX = fmax(0.5f, 1.0f - normalizedDistance);
        const float speedScaleY = fmax(0.5f, normalizedDistance);
        newXSpeed = fmin(MAX_BALL_SPEED, MAX_BALL_SPEED * speedScaleX + MAX_BALL_SPEED / 2.0f);
        newYSpeed = copysignf(MAX_BALL_SPEED * speedScaleY, _ySpeed);
    }
};

Ball &ball = Ball::getInstance();

Player leftPlayer(PLAYER_LEFT);
Player rightPlayer(PLAYER_RIGHT);

bool contentsOfFile(const std::string &filename, std::string &output) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        debugPrint("Could not open file %s", filename.c_str());
        return false;
    }

    std::stringstream ss;

    std::string line;
    while (ifs.good()) {
        std::getline(ifs, line);
        ss << line << std::endl;
    }

    output = ss.str();

    return true;
}

void initialize() {
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    std::string vertexShaderSource;
    if (!contentsOfFile("vertex.glsl", vertexShaderSource)) {
        exit(1);
    }

    std::string fragmentShaderSource;
    if (!contentsOfFile("fragment.glsl", fragmentShaderSource)) {
        exit(1);
    }

    Shader vertexShader(GL_VERTEX_SHADER);
    if (!vertexShader.compile(vertexShaderSource)) {
        debugPrint("Failed to compile vertex shader: %s", vertexShader.getCompileStatus().c_str());
        exit(1);
    }

    Shader fragmentShader(GL_FRAGMENT_SHADER);
    if (!fragmentShader.compile(fragmentShaderSource)) {
        debugPrint("Failed to compile fragment shader: %s", fragmentShader.getCompileStatus().c_str());
        exit(1);
    }

    Program shaderProgram(vertexShader, fragmentShader);
    if (!shaderProgram.link()) {
        debugPrint("Failed to link shader program: %s", shaderProgram.getLinkStatus().c_str());
        exit(1);
    }

    GLuint programID = shaderProgram.getGLID();
    glUseProgram(programID);

    GLint posAttrib = glGetAttribLocation(programID, "position");
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);

    GLint colorAttrib = glGetAttribLocation(programID, "color");
    glEnableVertexAttribArray(colorAttrib);
    glVertexAttribPointer(colorAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));

    glEnableVertexAttribArray(posAttrib);

    ball.setLeftPlayer(&leftPlayer);
    ball.setRightPlayer(&rightPlayer);
}

void verticesForRect(const Rect &rect, float vertices[], const Color &color, int offset) {
    // Left triangle
    vertices[0 + offset] = (rect.x / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[1 + offset] = ((-rect.y / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[2 + offset] = color.r;
    vertices[3 + offset] = color.g;
    vertices[4 + offset] = color.b;

    vertices[5 + offset] = ((rect.x + rect.width) / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[6 + offset] = ((-(rect.y + rect.height) / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[7 + offset] = color.r;
    vertices[8 + offset] = color.g;
    vertices[9 + offset] = color.b;
    
    vertices[10 + offset] = (rect.x / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[11 + offset] = ((-(rect.y + rect.height) / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[12 + offset] = color.r;
    vertices[13 + offset] = color.g;
    vertices[14 + offset] = color.b;

    // Right triangle
    vertices[15 + offset] = (rect.x / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[16 + offset] = ((-rect.y / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[17 + offset] = color.r;
    vertices[18 + offset] = color.g;
    vertices[19 + offset] = color.b;
    
    vertices[20 + offset] = ((rect.x + rect.width) / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[21 + offset] = ((-rect.y / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[22 + offset] = color.r;
    vertices[23 + offset] = color.g;
    vertices[24 + offset] = color.b;
    
    vertices[25 + offset] = ((rect.x + rect.width) / SCREEN_WIDTH * 2.0f) - 1.0f;
    vertices[26 + offset] = ((-(rect.y + rect.height) / SCREEN_HEIGHT) * 2.0f) + 1.0f;
    vertices[27 + offset] = color.r;
    vertices[28 + offset] = color.g;
    vertices[29 + offset] = color.b;
}

#define NUM_QUAD_ELEMENTS 30

void drawRectangles(const std::vector<Rect> &rects, const std::vector<Color> &colors) {
    if (rects.size() != colors.size()) {
        debugPrint("drawRectangles: mismatch between size of rectangles vector and colors vector.");
        exit(1);
    }

    const size_t numCoordinates = rects.size() * NUM_QUAD_ELEMENTS;
    float *vertices = new float[numCoordinates];

    for (size_t i = 0; i < rects.size(); i++) {
        verticesForRect(rects[i], vertices, colors[i], i * NUM_QUAD_ELEMENTS);
    }

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * numCoordinates, vertices, GL_DYNAMIC_DRAW);

    delete vertices;

    glDrawArrays(GL_TRIANGLES, 0, numCoordinates / 2);
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);

    std::vector<Rect> rectangles;

    Rect ballRect(0, 0, BALL_SIZE, BALL_SIZE);
    ball.getCoordinates(ballRect.x, ballRect.y);

    rectangles.push_back(ballRect);

    rectangles.push_back(leftPlayer.getCoords());
    rectangles.push_back(rightPlayer.getCoords());

    std::vector<Color> colors;
    colors.push_back(Color(1.0f, 1.0f, 1.0f));
    colors.push_back(Color(1.0f, 0.0f, 0.0f));
    colors.push_back(Color(0.0f, 0.0f, 1.0f));

    drawRectangles(rectangles, colors);
}

bool paused = true;

void update(float elapsed) {
    if (!paused) {
        ball.update(elapsed);
        leftPlayer.update(elapsed);
        rightPlayer.update(elapsed);
    }
}

void KeyboardCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
#define PRESSED(k) (key == k && action == GLFW_PRESS)
#define RELEASED(k) (key == k && action == GLFW_RELEASE)

    // Left player controls
    if (PRESSED(GLFW_KEY_W)) {
        leftPlayer.setVerticalSpeed(-PLAYER_SPEED);
    }

    if (RELEASED(GLFW_KEY_W)) {
        leftPlayer.setVerticalSpeed(0.0f);
    }

    if (PRESSED(GLFW_KEY_S)) {
        leftPlayer.setVerticalSpeed(PLAYER_SPEED);
    }

    if (RELEASED(GLFW_KEY_S)) {
        leftPlayer.setVerticalSpeed(0.0f);
    }

    // Right player controls
    if (PRESSED(GLFW_KEY_UP)) {
        rightPlayer.setVerticalSpeed(-PLAYER_SPEED);
    }

    if (RELEASED(GLFW_KEY_UP)) {
        rightPlayer.setVerticalSpeed(0.0f);
    }

    if (PRESSED(GLFW_KEY_DOWN)) {
        rightPlayer.setVerticalSpeed(PLAYER_SPEED);
    }

    if (RELEASED(GLFW_KEY_DOWN)) {
        rightPlayer.setVerticalSpeed(0.0f);
    }

    // General game controls
    if (PRESSED(GLFW_KEY_SPACE)) {
        paused = !paused;
    }

    if (PRESSED(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(window, 1);
    }
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "GLPong", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, KeyboardCallback);

    // Make the window's context current
    glfwMakeContextCurrent(window);

    GLenum result = glewInit();
    if (result != GLEW_OK) {
        debugPrint("Failed to initialize GLEW: %s", glewGetErrorString(result));
        return -1;
    }

    initialize();

    double lastTime = glfwGetTime();

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window)) {
        if (shouldUpdateTitle) {
            const char fmt[] = "Red: %i, Blue: %i";
            const int len = snprintf(NULL, 0, fmt, leftPlayer.getScore(), rightPlayer.getScore()) + 2;
            char *title = new char[len];
            snprintf(title, len, fmt, leftPlayer.getScore(), rightPlayer.getScore());
            glfwSetWindowTitle(window, title);
            delete[] title;
            shouldUpdateTitle = false;
        }

        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastTime;

        render();
        update(static_cast<float>(elapsed));

        // Swap font and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();

        lastTime = currentTime;
    }

    glfwTerminate();
    return 0;
}
