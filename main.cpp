#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <exception>
#include <fstream>
#include <sstream>

// Displays a window with a square inside.

void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	glBegin(GL_POLYGON);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(0.5, 0.0, 0.0);
		glVertex3f(0.5, 0.5, 0.0);
		glVertex3f(0.0, 0.5, 0.0);
	glEnd();

	glFlush();
}

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
    std::vector<int> vertexIndices;
    std::vector<int> textureCoordinateIndices;
    std::vector<int> normalIndices;
};

// l
struct ObjLine
{
    std::vector<int> vertexIndices;
};

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

ObjVertex parseVertex(const std::string &line)
{
    auto tokens = split(line, ' ');

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
    auto tokens = split(line, ' ');

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
    auto tokens = split(line, ' ');

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
    auto tokens = split(line, ' ');

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

// https://www.cs.cmu.edu/~mbz/personal/graphics/obj.html
// https://en.wikipedia.org/wiki/Wavefront_.obj_file#File_format
class ObjectFile
{
    private:
        std::string _filename;

        std::vector<ObjVertex> _vertices;
        std::vector<ObjTextureCoordinate> _texcoords;
        std::vector<ObjNormal> _normals;
        std::vector<ObjParameterSpaceVertex> _paramSpaceVertices;
        std::vector<ObjFace> _faces;
        std::vector<ObjLine> _lines;

        size_t _verticesCount = 1;
        size_t _texcoordsCount = 1;
        size_t _normalsCount = 1;

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

                if (line.size() == 0)
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
                    _faces.push_back(parseFace(line));
                } /* else if (identifier == "l ") {
                    _lines.push_back(parseLine(line));
                } */ else if (identifier.at(0) != '#') {
                    throw InvalidObjFileException("unknown token " + identifier + " on line " + std::to_string(lineNum));
                }

            }


        }
};


void usage()
{
    std::cerr << "fuck you and use a file.obj" << std::endl;
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

    try {
        ObjectFile obj(filename.c_str());
    } catch (std::exception &e) {
        std::cerr << "Cannot parse file " << filename << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // create window using glfw
    GLFWwindow* window;
    if (!glfwInit())
        return -1;

    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window))
    {
        display();
        glfwSwapBuffers(window);

        glfwPollEvents();
    }


    glfwTerminate();
    return 0;
}