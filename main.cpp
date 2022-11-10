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

    public:
        ObjectFile(const char* filename): _filename(filename)
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
};


void usage()
{
    std::cerr << "fuck you and use a file.obj" << std::endl;
}

// function to rotate a given vertex about the origin by a given angle in degrees
glm::vec3 rotateVertex(glm::vec3 vertex, float angleX, float angleY, float angleZ)
{
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(angleX), glm::vec3(1.0f, 0.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(angleY), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(angleZ), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec4 rotatedVertex = rotationMatrix * glm::vec4(vertex, 1.0f);
    return glm::vec3(rotatedVertex);
}

void displayObject(ObjectFile &obj)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (auto &face: obj._faces)
    {
        glBegin(GL_POLYGON);
        for (auto &objVertex: face.vertices)
        {
            // if (vertex.normalIndex.has_value())
            //     glNormal3f(obj._normals.at(vertex.normalIndex.value()).x, obj._normals.at(vertex.normalIndex.value()).y, obj._normals.at(vertex.normalIndex.value()).z);
            // if (vertex.textureCoordinateIndex.has_value())
            //     glTexCoord2f(obj._texcoords.at(vertex.textureCoordinateIndex.value()).u, obj._texcoords.at(vertex.textureCoordinateIndex.value()).v);

            glm::vec3 vertex = glm::vec3(obj._vertices.at(objVertex.vertexIndex).x, obj._vertices.at(objVertex.vertexIndex).y, obj._vertices.at(objVertex.vertexIndex).z);
            vertex = rotateVertex(vertex, 45, 30, 45);
            glVertex3f(vertex.x, vertex.y, vertex.z);
            // glVertex3f(obj._vertices.at(vertex.vertexIndex).x, obj._vertices.at(vertex.vertexIndex).y, obj._vertices.at(vertex.vertexIndex).z);
        }
        glEnd();

        glFlush();
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        usage();
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    // check if .obj
    if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".obj")
    {
        usage();
        return EXIT_FAILURE;
    }

    std::unique_ptr<ObjectFile> obj;

    try {
        obj = std::make_unique<ObjectFile>(filename.c_str());
    } catch (std::exception &e) {
        std::cerr << "Cannot parse file " << filename << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

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

    while (!glfwWindowShouldClose(window))
    {
        displayObject(*obj);
        glfwSwapBuffers(window);

        glfwPollEvents();
    }


    glfwTerminate();
    return 0;
}