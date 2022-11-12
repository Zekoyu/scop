#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <exception>
#include <fstream>
#include <sstream>
#include <optional>
#include <chrono>
#include <thread>


#define TARGET_FPS 60

struct FileNotFoundException : public std::exception
{
    private:
        std::string message;

    public:
        FileNotFoundException(const std::string& message) : message(message) {}
        virtual const char* what() const throw() { return message.c_str(); }
};

struct InvalidObjFileException : public std::exception
{
    private:
        std::string message;

    public:
        InvalidObjFileException(const std::string& message) : message(message) {}
        virtual const char* what() const throw() { return message.c_str(); }
};

// v
struct ObjVertex
{
    double x, y, z;
    double w = 1.0;
};

// vt
struct ObjTextureCoordinate
{
    double u;
    double v = 0.0, w = 0.0;
};

// vn
struct ObjNormal
{
    double x, y, z;
};

// vp
struct ObjParameterSpaceVertex
{
    double u;
    double v = 0.0, w = 1.0;
};

// f
struct ObjFace
{
    struct Vertex
    {
        int vertexIndex;
        std::optional<int> textureCoordinateIndex;
        std::optional<int> normalIndex;
    };

    std::vector<Vertex> vertices;
};

// l
struct ObjLine
{
    std::vector<std::size_t> vertexIndices;
};

// split("a/b/c//d", '/') -> {"a", "b", "c", "", "d"}
std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

// split_without_empty("a/b/c//d", '/') -> {"a", "b", "c", "d"}
std::vector<std::string> split_without_empty(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
        if (!token.empty())
            tokens.push_back(token);
    return tokens;
}

// Rotate a vertex around a given point
glm::vec3 rotateVertex(const ObjVertex &vertex, const glm::vec3 &center, float angleX, float angleY, float angleZ)
{
    angleX = glm::radians(angleX);
    angleY = glm::radians(angleY);
    angleZ = glm::radians(angleZ);
    
    // change the origin to the center
    glm::vec3 v = {vertex.x - center.x, vertex.y - center.y, vertex.z - center.z};

    // rotate around x
    glm::vec3 v2 = {v.x, v.y * cos(angleX) - v.z * sin(angleX), v.y * sin(angleX) + v.z * cos(angleX)};
    // rotate around y
    glm::vec3 v3 = {v2.x * cos(angleY) + v2.z * sin(angleY), v2.y, -v2.x * sin(angleY) + v2.z * cos(angleY)};
    // rotate around z
    glm::vec3 v4 = {v3.x * cos(angleZ) - v3.y * sin(angleZ), v3.x * sin(angleZ) + v3.y * cos(angleZ), v3.z};

    // change the origin back
    return {v4.x + center.x, v4.y + center.y, v4.z + center.z};
}

ObjVertex parseVertex(const std::string &line)
{
    auto tokens = split_without_empty(line, ' ');

    if (tokens.size() < 4 || tokens.size() > 5 || tokens.at(0) != "v")
        throw InvalidObjFileException("Invalid vertex line: " + line);

    ObjVertex vertex;

    try {
        vertex.x = std::stod(tokens.at(1));
        vertex.y = std::stod(tokens.at(2));
        vertex.z = std::stod(tokens.at(3));
        if (tokens.size() >= 5)
            vertex.w = std::stod(tokens.at(4));
    } catch (std::invalid_argument &e) {
        throw InvalidObjFileException("Invalid vertex line (invalid values): " + line);
    }

    return vertex;
}

ObjTextureCoordinate parseTextureCoordinate(const std::string &line)
{
    auto tokens = split_without_empty(line, ' ');

    if (tokens.size() < 2 || tokens.size() > 4 || tokens.at(0) != "vt")
        throw InvalidObjFileException("Invalid texture coordinate line: " + line);

    ObjTextureCoordinate textureCoordinate;
    try {
        textureCoordinate.u = std::stod(tokens.at(1));
        if (tokens.size() >= 3)
            textureCoordinate.v = std::stod(tokens.at(2));
        if (tokens.size() >= 4)
            textureCoordinate.w = std::stod(tokens.at(3));
        return textureCoordinate;
    } catch (std::invalid_argument &e) {
        throw InvalidObjFileException("Invalid texture coordinate line (invalid values): " + line);
    }

    return textureCoordinate;
}

ObjNormal parseNormal(const std::string &line)
{
    auto tokens = split_without_empty(line, ' ');

    if (tokens.size() != 4 || tokens.at(0) != "vn")
        throw InvalidObjFileException("Invalid normal line: " + line);

    ObjNormal normal;
    try {
        normal.x = std::stod(tokens.at(1));
        normal.y = std::stod(tokens.at(2));
        normal.z = std::stod(tokens.at(3));
        return normal;
    } catch (std::invalid_argument &e) {
        throw InvalidObjFileException("Invalid normal line (invalid values): " + line);
    }

    return normal;
}

ObjParameterSpaceVertex parseParameterSpaceVertex(const std::string &line)
{
    auto tokens = split_without_empty(line, ' ');

    if (tokens.size() < 2 || tokens.size() > 4 || tokens.at(0) != "vp")
        throw InvalidObjFileException("Invalid parameter space vertex line: " + line);

    ObjParameterSpaceVertex parameterSpaceVertex;
    try {
        parameterSpaceVertex.u = std::stod(tokens.at(1));
        if (tokens.size() >= 3)
            parameterSpaceVertex.v = std::stod(tokens.at(2));
        if (tokens.size() >= 4)
            parameterSpaceVertex.w = std::stod(tokens.at(3));
        return parameterSpaceVertex;
    } catch (std::invalid_argument &e) {
        throw InvalidObjFileException("Invalid parameter space vertex line (invalid values): " + line);
    }

    return parameterSpaceVertex;
}

ObjFace parseFace(const std::string &line)
{
    auto tokens = split_without_empty(line, ' ');

    if (tokens.size() < 4 || tokens.at(0) != "f")
        throw InvalidObjFileException("Invalid face line: " + line);

    ObjFace face;
    try
    {
        for (int i = 1; i < tokens.size(); i++)
        {
            auto subtokens = split(tokens.at(i), '/');
            if (subtokens.size() < 1 || subtokens.size() > 3)
                throw InvalidObjFileException("Invalid face line: " + line);

            ObjFace::Vertex vertex;
            vertex.vertexIndex = std::stoi(subtokens.at(0));
            // vertex normal without texture coordinate is f v1//vn1
            if (subtokens.size() >= 2 && subtokens.at(1) != "")
                vertex.textureCoordinateIndex = std::stoi(subtokens.at(1));
            if (subtokens.size() >= 3)
                vertex.normalIndex = std::stoi(subtokens.at(2));

            face.vertices.push_back(vertex);
        }
    }
    catch (std::invalid_argument &e)
    {
        throw InvalidObjFileException("Invalid face line (invalid values): " + line);
    }

    return face;
}

// https://www.cs.cmu.edu/~mbz/personal/graphics/obj.html
// https://en.wikipedia.org/wiki/Wavefront_.obj_file#File_format
// http://paulbourke.net/dataformats/obj/
class ObjectFile
{
    public:
        std::string _filename;

        std::vector<ObjVertex> _vertices;
        std::vector<ObjTextureCoordinate> _texcoords;
        std::vector<ObjNormal> _normals;
        std::vector<ObjParameterSpaceVertex> _paramSpaceVertices;
        std::vector<ObjFace> _faces;
        std::vector<ObjLine> _lines;

        size_t _verticesCount = 0;
        size_t _texcoordsCount = 0;
        size_t _normalsCount = 0;

        double _scale = 1.0;

    public:
        ObjectFile() {}

        ObjectFile(const char* filename): _filename(filename)
        {
            load(filename);
            normalize();
        }

        void load(const char* filename)
        {
            std::cout << "Loading " << filename << std::endl;

            std::ifstream file(filename);
            if (!file.is_open())
                throw FileNotFoundException("file " + _filename + " not found");

            std::string line;
            size_t lineNum = 0;
            while (std::getline(file, line))
            {
                lineNum++;

                if (line.size() == 0 || line.at(0) == '#')
                    continue;

                if (line.size() < 2)
                    throw InvalidObjFileException("line " + std::to_string(lineNum) + " is invalid (too short)");

                std::string identifier = line.substr(0, 2);
                if (identifier == "v ") {
                    _vertices.push_back(parseVertex(line));
                     _verticesCount++;
                } else if (identifier == "vt") {
                    _texcoords.push_back(parseTextureCoordinate(line));
                    _texcoordsCount++;
                } else if (identifier == "vn") {
                    _normals.push_back(parseNormal(line));
                    _normalsCount++;
                } else if (identifier == "vp") {
                    _paramSpaceVertices.push_back(parseParameterSpaceVertex(line));
                }  else if (identifier == "f ") {
                    ObjFace face = parseFace(line);


                    // Converts index to 0-based indexes + if negative it means it's relative to the end of the array (eg. -1 is the last element)
                    for (auto &vertex: face.vertices)
                    {
                        if (vertex.vertexIndex < 0)
                            vertex.vertexIndex = _verticesCount + vertex.vertexIndex;
                        else
                            vertex.vertexIndex--;

                        if (vertex.textureCoordinateIndex.has_value() && vertex.textureCoordinateIndex.value() < 0)
                            vertex.textureCoordinateIndex = _texcoordsCount + vertex.textureCoordinateIndex.value();
                        else if (vertex.textureCoordinateIndex.has_value())
                            vertex.textureCoordinateIndex.value()--;

                        if (vertex.normalIndex.has_value() && vertex.normalIndex.value() < 0)
                            vertex.normalIndex = _normalsCount + vertex.normalIndex.value();
                        else if (vertex.normalIndex.has_value())
                            vertex.normalIndex.value()--;

                        if (vertex.vertexIndex < 0 || vertex.vertexIndex >= _verticesCount)
                            throw InvalidObjFileException("line " + std::to_string(lineNum) + " is invalid (vertex index out of bounds)");
                        else if (vertex.textureCoordinateIndex.has_value() && (vertex.textureCoordinateIndex.value() < 0 || vertex.textureCoordinateIndex.value() >= _texcoordsCount))
                            throw InvalidObjFileException("line " + std::to_string(lineNum) + " is invalid (texture coordinate index out of bounds)");
                        else if (vertex.normalIndex.has_value() && (vertex.normalIndex.value() < 0 || vertex.normalIndex.value() >= _normalsCount))
                            throw InvalidObjFileException("line " + std::to_string(lineNum) + " is invalid (normal index out of bounds)");
                    }

                    _faces.push_back(face);
                } /* else if (identifier == "l ") {
                    _lines.push_back(parseLine(line));
                } */ else {
                    throw InvalidObjFileException("unknown token " + identifier + " on line " + std::to_string(lineNum));
                }

            }

            std::cout << "Successfully loaded and parsed " << filename << std::endl;
        }

        void display()
        {
            size_t currentFace = 0;
            for (auto &face: _faces)
            {
                currentFace++;
                
                glBegin(GL_POLYGON);
                for (auto &objVertex: face.vertices)
                {
                    float r = (currentFace * 0.1f);
                    if (r > 1.0f)
                        r -= (int)r;
                    float g = (currentFace * 0.2f);
                    if (g > 1.0f)
                        g -= (int)g;
                    float b = (currentFace * 0.3f);
                    if (b > 1.0f)
                        b -= (int)b;

                    glColor3f(r, g, b);

                    if (objVertex.normalIndex.has_value())
                        glNormal3f(_normals.at(objVertex.normalIndex.value()).x, _normals.at(objVertex.normalIndex.value()).y, _normals.at(objVertex.normalIndex.value()).z);
                    if (objVertex.textureCoordinateIndex.has_value())
                        glTexCoord2f(_texcoords.at(objVertex.textureCoordinateIndex.value()).u, _texcoords.at(objVertex.textureCoordinateIndex.value()).v);

                    glm::vec3 vertex = glm::vec3(_vertices.at(objVertex.vertexIndex).x, _vertices.at(objVertex.vertexIndex).y, _vertices.at(objVertex.vertexIndex).z);
                    glVertex3f(vertex.x, vertex.y, vertex.z);
                }
                glEnd();

            }
            glFlush();
        }

        glm::vec3 getCenterPoint()
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            for (const auto &vertex: _vertices)
            {
                x += vertex.x;
                y += vertex.y;
                z += vertex.z;
            }

            return glm::vec3(x / _vertices.size(), y / _vertices.size(), z / _vertices.size());
        }

        void rotate(float angleXDeg, float angleYDeg, float angleZDeg)
        {
            glm::vec3 centerPoint = getCenterPoint();

            for (auto &vertex: _vertices)
            {
                glm::vec3 rotatedVertex = rotateVertex(vertex, centerPoint, angleXDeg, angleYDeg, angleZDeg);
                vertex.x = rotatedVertex.x;
                vertex.y = rotatedVertex.y;
                vertex.z = rotatedVertex.z;
            }
        }

        void translate(float x, float y, float z)
        {
            for (auto &vertex: _vertices)
            {
                vertex.x += x;
                vertex.y += y;
                vertex.z += z;
            }
        }

        void scale(float factor)
        {
            if (factor == 0.0f || _scale * factor < 0.01f || _scale * factor > 2.0f)
                return;
                
            _scale *= factor;
            glm::vec3 centerPoint = getCenterPoint();

            for (auto &vertex: _vertices)
            {
                vertex.x = centerPoint.x + (vertex.x - centerPoint.x) * factor;
                vertex.y = centerPoint.y + (vertex.y - centerPoint.y) * factor;
                vertex.z = centerPoint.z + (vertex.z - centerPoint.z) * factor;
            }
        }

        void center()
        {
            glm::vec3 centerPoint = getCenterPoint();

            for (auto &vertex: _vertices)
            {
                vertex.x -= centerPoint.x;
                vertex.y -= centerPoint.y;
                vertex.z -= centerPoint.z;
            }
        }

        // Make all vertices coordinates between -1 and 1
        void normalize()
        {
            glm::vec3 centerPoint = getCenterPoint();

            float max = 0.0f;
            for (const auto &vertex: _vertices)
            {
                if (std::abs(vertex.x) > max)
                    max = std::abs(vertex.x);
                if (std::abs(vertex.y) > max)
                    max = std::abs(vertex.y);
                if (std::abs(vertex.z) > max)
                    max = std::abs(vertex.z);
            }

            for (auto &vertex: _vertices)
            {
                vertex.x = (vertex.x - centerPoint.x) / max;
                vertex.y = (vertex.y - centerPoint.y) / max;
                vertex.z = (vertex.z - centerPoint.z) / max;
            }
        }
};


void usage()
{
    std::cerr << "fuck you and use a file.obj" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage();
        return EXIT_FAILURE;
    }

    // std::unique_ptr<ObjectFile[]> objs = std::make_unique<ObjectFile[]>(argc);
    ObjectFile** objs = new ObjectFile*[argc];
    // objs[0] = NULL;

    for (int i = 1; i < argc; i++)
    {
        std::string filename = argv[i];

        // check if .obj
        if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".obj")
        {
            usage();
            return EXIT_FAILURE;
        }

        try {
            objs[i - 1] = new ObjectFile(filename.c_str());
        } catch (std::exception &e) {
            std::cerr << "Cannot parse file " << filename << ": " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    objs[argc - 1] = nullptr;


    // create window using glfw
    GLFWwindow* window;
    if (!glfwInit())
        return -1;


    window = glfwCreateWindow(640, 640, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glEnable(GL_DEPTH_TEST);
    
    glDepthFunc(GL_LESS);

    glfwSetWindowUserPointer(window, objs);


    // hide cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Capture obj to be able to use it in the callback
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
        static double last_xpos = xpos;
        static double last_ypos = ypos;

        double y_offset = last_ypos - ypos;
        double x_offset = xpos - last_xpos;

        auto objs = static_cast<ObjectFile**>(glfwGetWindowUserPointer(window));
        // idk why x axis is vertical and y axis is horizontal
        for (size_t i = 0; objs[i] != nullptr; i++)
        {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
                objs[i]->translate(x_offset / 250.0, y_offset / 250.0, 0.0);
            else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
                objs[i]->scale(1.0f + y_offset / 100.0);
            else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                objs[i]->rotate(y_offset * 0.5, x_offset * 0.5, 0.0);
        }
        //     (objs[i])->rotate((y_offset) * 0.1f, (x_offset * 0.1f, 0.0f);

        last_xpos = xpos;
        last_ypos = ypos;
    });

    glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset)
    {
        double zoomFactor = 1.0f + (yoffset / 10.0f);

        auto objs = static_cast<ObjectFile**>(glfwGetWindowUserPointer(window));
        for (size_t i = 0; objs[i] != nullptr; i++)
            objs[i]->scale(zoomFactor);
    });

    while (!glfwWindowShouldClose(window))
    {
        auto frame_start = std::chrono::high_resolution_clock::now();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

        for (int i = 1; i < argc; i++)
            (objs[i - 1])->display();

        glfwSwapBuffers(window);

        // close on ESC press
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glfwPollEvents();

        for (int i = 1; i < argc; i++)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                (objs[i - 1])->translate(0.0f, -0.025f, 0.0f);
            
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                (objs[i - 1])->translate(0.0f, 0.025f, 0.0f);
            
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                (objs[i - 1])->translate(0.025f, 0.0f, 0.0f);

            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                (objs[i - 1])->translate(-0.025f, 0.0f, 0.0f);

            // rotate on keypress (in degrees)
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                (objs[i - 1])->rotate(1.5f, 0.0f, 0.0f);

            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                (objs[i - 1])->rotate(-1.5f, 0.0f, 0.0f);

            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
                (objs[i - 1])->rotate(0.0f, 1.5f, 0.0f);

            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
                (objs[i - 1])->rotate(0.0f, -1.5f, 0.0f);

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                (objs[i - 1])->rotate(0.0f, 0.0f, 1.5f);

            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                (objs[i - 1])->rotate(0.0f, 0.0f, -1.5f);

            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                (objs[i - 1])->center();
        }

        auto frame_end = std::chrono::high_resolution_clock::now();

        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);

        // std::cout << "Frame end: " << frame_duration.count() << "ms" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / TARGET_FPS) - frame_duration);
    }


    glfwTerminate();
    return 0;
}