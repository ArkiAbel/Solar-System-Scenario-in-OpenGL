#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <random>

using namespace std;

bool fKeyPressed = false;

//keparany
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

//alab allapot beallitasok
bool gameOver = false;
bool flashlightOn = false;
float respawnTimer = 0.0f;

//kameranezet, pozicio
glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 15.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

//gorgomozdulatok a konnyabb navigalashoz
float cameraAcceleration = 1.0f;
const float MIN_ACCELERATION = 0.1f;
const float MAX_ACCELERATION = 5.0f;
const float ACCELERATION_STEP = 0.2f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 sunPos(0.0f, 0.0f, 0.0f);

//egitestek adatstrukturaja
struct CelestialBody {
    glm::vec3 position;
    float size;
    float collisionRadius;
};

//bolygo adatai
struct Planet {
    float distance;
    float angle;
    float speed;
    float rotation;
    float rotationSpeed;
    float size;
    glm::vec3 color;
    //arnyalas
    float darkness;
    CelestialBody body;
    unsigned int textureID;
    std::string texturePath;
};

//bolygok tombje, a nap mint csillag pedig kulon, mivel neki mas tulajdonsagai vannak...
vector<Planet> planets;
CelestialBody sun;

//cubemap megoldas, ahogy a hazi leirasban volt
float skyboxVertices[] = {
    //definialasa a ponthalmaznak       
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

//shaderek inicializalasa a kulonbozo egitestek es a szerepuk alapjan
const char* vertexShaderSource = R"( //vertexshader
    #version 400 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    out vec3 FragPos;
    out vec3 LocalPos;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        LocalPos = aPos;
        TexCoord = aTexCoord;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"( //fragmentshader
    #version 400 core
    uniform sampler2D planetTexture;
    in vec3 FragPos;
    in vec2 TexCoord;
    in vec3 LocalPos;

    //-----------------------a material beallitasok
    uniform vec3 objectColor;
    uniform vec3 sunPos;
    uniform vec3 sunColor;
    uniform vec3 cameraPos;
    uniform vec3 cameraLightColor;
    uniform float ambientStrength;
    uniform float specularStrength;
    uniform float darkness;
    uniform bool flashlightOn;
    out vec4 FragColor;

    void main() {
        //------------------------a textura mapping, melyet egy gomb feluletenek feleltetunk meg, es egy kockara teritjuk szet
        vec3 norm = normalize(LocalPos);
        float theta = atan(norm.z, norm.x); //azimut
        float phi = acos(norm.y); //emelkedesi szog
        vec2 uv;
        //normalizalasok 0-1-es skalara
        uv.x = (theta + 3.14159265359) / (2.0 * 3.14159265359);
        uv.y = phi / 3.14159265359;
        
        vec4 texColor = texture(planetTexture, uv);

        vec3 ambient = ambientStrength * sunColor; //ambiens feny
        vec3 surfaceNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
        vec3 lightDir = normalize(sunPos - FragPos);

        float diff = max(dot(surfaceNormal, lightDir), 0.0);

        vec3 diffuse = diff * sunColor * (1.0 - darkness);
        vec3 viewDir = normalize(cameraPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, surfaceNormal);

        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

        vec3 specular = specularStrength * spec * sunColor;
        vec3 cameraDiffuse = vec3(0.0);
        vec3 cameraSpecular = vec3(0.0);

        if (flashlightOn) { //lampafeny bekapcsolt allapot kezelese
            vec3 cameraLightDir = normalize(FragPos - cameraPos);

            float distance = length(cameraPos - FragPos);
            float attenuation = 2.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
            float camDiff = max(dot(surfaceNormal, -cameraLightDir), 0.0);

            cameraDiffuse = attenuation * camDiff * cameraLightColor * 2.0;
            vec3 cameraReflectDir = reflect(cameraLightDir, surfaceNormal);
            float camSpec = pow(max(dot(viewDir, cameraReflectDir), 0.0), 32.0);

            cameraSpecular = attenuation * specularStrength * camSpec * cameraLightColor * 2.0;
        }

        vec3 result = (ambient + diffuse + specular + cameraDiffuse + cameraSpecular) * texColor.rgb;
        FragColor = vec4(result, 1.0);
    }
)";

const char* sunFragmentShaderSource = R"( //Nap fragmentshader
    #version 400 core
    out vec4 FragColor;
    uniform vec3 objectColor;

    void main() {
        FragColor = vec4(objectColor, 1.0);
    }
)";

const char* skyboxVertexShaderSource = R"( //skybox shader
    #version 400 core
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;

    uniform mat4 projection;
    uniform mat4 view;

    void main() {
        TexCoords = aPos;
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
    }  
)";

const char* skyboxFragmentShaderSource = R"( //fragmentshader
    #version 400 core
    in vec3 TexCoords;
    out vec4 FragColor;

    uniform sampler2D skyboxTexture;

    void main() {

        vec3 norm = normalize(TexCoords);
        vec2 uv;
        
        vec3 absNorm = abs(norm);

        if (absNorm.x >= absNorm.y && absNorm.x >= absNorm.z) {
            uv.x = norm.x > 0.0 ? -norm.z : norm.z;
            uv.y = -norm.y;
            uv = (uv + 1.0) * 0.5;
        } else if (absNorm.y >= absNorm.x && absNorm.y >= absNorm.z) {
            uv.x = norm.x;
            uv.y = norm.y > 0.0 ? norm.z : -norm.z;
            uv = (uv + 1.0) * 0.5;
        } else {
            uv.x = norm.z > 0.0 ? norm.x : -norm.x;
            uv.y = -norm.y;
            uv = (uv + 1.0) * 0.5;
        }

        vec4 texColor = texture(skyboxTexture, uv);
        FragColor = texColor; //emissziv textura
    }
)";

//elozo hazibol atemelt shaderprogram fuggveny minimalis modositassal
unsigned int createShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource) {

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

//textura betolto fuggveny a bolygokra
GLuint loadTexture(const char* filename) {

    std::string path = filename;
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

    //hianyzo repo kezelese
    if (!data) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return 0;
    }

    //stb_image header hasznalata, mint a gyakorlaton
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    return texture;
}

float dist = 3.0f;
//tavolsag szorzo

void initPlanets() { //bolygok inicializalasa

    sun.position = glm::vec3(0.0f);
    sun.size = 1.2f;
    sun.collisionRadius = 1.2f * 1.5f;
    float multiplier = 10.0f; //tavolsag skalazo

    //kulonbozo naprendszer bolygok be "push-olasa" a vektorba
    planets.push_back({ dist * 2.0f, 0.0f, multiplier * 1.5f, 0.0f, multiplier * 1.5f, 0.4f, glm::vec3(0.7f, 0.6f, 0.5f), 0.1f, CelestialBody{}, 0, "mercury_UV.jpg" });
    planets.push_back({ dist * 3.0f, 0.0f, multiplier * 1.2f, 0.0f, multiplier * 1.2f, 0.6f, glm::vec3(0.9f, 0.7f, 0.4f), 0.2f, CelestialBody{}, 0, "venus_UV.jpg" });
    planets.push_back({ dist * 4.0f, 0.0f, multiplier * 1.0f, 0.0f, multiplier * 1.0f, 0.6f, glm::vec3(0.2f, 0.4f, 0.9f), 0.3f, CelestialBody{}, 0, "earth_UV.jpg" });
    planets.push_back({ dist * 5.0f, 0.0f, multiplier * 0.8f, 0.0f, multiplier * 0.8f, 0.5f, glm::vec3(0.8f, 0.3f, 0.2f), 0.4f, CelestialBody{}, 0, "mars_UV.jpg" });
    planets.push_back({ dist * 6.5f, 0.0f, multiplier * 0.6f, 0.0f, multiplier * 0.6f, 1.0f, glm::vec3(0.8f, 0.6f, 0.4f), 0.5f, CelestialBody{}, 0, "jupiter_UV.jpg" });
    planets.push_back({ dist * 8.0f, 0.0f, multiplier * 0.5f, 0.0f, multiplier * 0.5f, 0.9f, glm::vec3(0.9f, 0.8f, 0.5f), 0.6f, CelestialBody{}, 0, "saturn_UV.jpg" });
    planets.push_back({ dist * 9.5f, 0.0f, multiplier * 0.4f, 0.0f, multiplier * 0.4f, 0.7f, glm::vec3(0.5f, 0.8f, 0.9f), 0.7f, CelestialBody{}, 0, "uranus_UV.jpg" });
    planets.push_back({ dist * 11.0f, 0.0f, multiplier * 0.3f, 0.0f, multiplier * 0.3f, 0.7f, glm::vec3(0.2f, 0.3f, 0.9f), 0.8f, CelestialBody{}, 0, "neptune_UV.jpg" });
    
    for (auto& planet : planets) {
        planet.body.size = planet.size;
        planet.body.collisionRadius = planet.size * 1.2f;
        planet.textureID = loadTexture(planet.texturePath.c_str());
    }
}

void loadPlanetTextures() {
    for (auto& planet : planets) {
        planet.textureID = loadTexture(planet.texturePath.c_str()); //parse -> konvertalas string-bol char-ba
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (gameOver) {
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            gameOver = false;
            cameraPos = glm::vec3(0.0f, 5.0f, 15.0f);
            cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
            //szogek visszaallitasa
            yaw = -90.0f;
            pitch = 0.0f;
        }
        return;
    }

    //accel
    float baseSpeed = 2.5f * deltaTime;
    float currentSpeed = baseSpeed * cameraAcceleration;
    //billentyu esemenyek kezelese
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraPos -= cameraUp * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraUp * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed) { flashlightOn = !flashlightOn; fKeyPressed = true; }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) { fKeyPressed = false; }
}

//eger esemenyek eszlelesere fuggveny
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    //valtozok modositasa az egermozgatas hatasara
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    float sensitivity = 0.1f;

    xoffset *= sensitivity; //a forgatas erzekenyseg
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// a mozgatas sebesseget lehet allitani a gorgozessel a konnyebb navigalashoz
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraAcceleration += yoffset * ACCELERATION_STEP;
    cameraAcceleration = glm::clamp(cameraAcceleration, MIN_ACCELERATION, MAX_ACCELERATION);
}

//utkozes eszlelesere valo fuggveny
bool checkCollision(const glm::vec3& pos1, float radius1, const glm::vec3& pos2, float radius2) {
    float distance = glm::length(pos1 - pos2);
    return distance < (radius1 + radius2 * 1.05f); //hitbox
}

void renderText(const string& text, float x, float y, float scale) {
    cout << text << endl;
}

int main() {

    float cameraZ = 25.0f;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solar System", nullptr, nullptr);

    if (window == nullptr) {
        cout << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1; //hibakod
    }

    glEnable(GL_DEPTH_TEST);
    unsigned int planetShaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int sunShaderProgram = createShaderProgram(vertexShaderSource, sunFragmentShaderSource);
    unsigned int skyboxShaderProgram = createShaderProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);

    float vertices[] = {
        //vetrexek es textura koordinatak egyeztetese a gyakorlati anyag alapjan (elso harom - utolso ketto)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,

         0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f,

         -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
          0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
          0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
          0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

         -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
          0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
          0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
         -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    //kezdeti bind VAO, VBO egitestekhez
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //kezdeti bind VAO, VBO a cubemap-hoz
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint skyboxTexture = loadTexture("skybox_UV.jpg");
    initPlanets();

    //render loop, ESC-re megszakithato
    while (!glfwWindowShouldClose(window)) {

        float currentFrame = glfwGetTime();

        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShaderProgram);
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, skyboxTexture);
        glUniform1i(glGetUniformLocation(skyboxShaderProgram, "skyboxTexture"), 0);
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        if (gameOver) {
            renderText("You Collided with a planet - ENTER to respawn", SCR_WIDTH / 2 - 100, SCR_HEIGHT / 2, 1.0f);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        glUseProgram(sunShaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, sun.position);
        model = glm::scale(model, glm::vec3(sun.size));

        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(sunShaderProgram, "objectColor"), 1.0f, 0.9f, 0.5f);

        //folyamatos binding -> bolygokra
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glUseProgram(planetShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(planetShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(planetShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(planetShaderProgram, "sunColor"), 1.0f, 1.0f, 0.8f);
        glUniform3f(glGetUniformLocation(planetShaderProgram, "sunPos"), sunPos.x, sunPos.y, sunPos.z);
        glUniform3f(glGetUniformLocation(planetShaderProgram, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(planetShaderProgram, "cameraLightColor"), 0.8f, 0.8f, 1.0f);
        glUniform1f(glGetUniformLocation(planetShaderProgram, "ambientStrength"), 0.2f);
        glUniform1f(glGetUniformLocation(planetShaderProgram, "specularStrength"), 0.5f);
        glUniform1i(glGetUniformLocation(planetShaderProgram, "flashlightOn"), flashlightOn);

        for (auto& planet : planets) { //keringetes a Nap korul & forgatasok sajat tengelyek korul
            planet.body.size = planet.size;
            planet.body.collisionRadius = planet.size * 1.0f;
            planet.angle += planet.speed * deltaTime;

            if (planet.angle > 360) planet.angle -= 360;
            planet.rotation += planet.rotationSpeed * deltaTime;

            if (planet.rotation > 360) planet.rotation -= 360;
            glm::vec3 planetPos = glm::vec3(
                planet.distance * cos(glm::radians(planet.angle)), 0.0f, planet.distance * sin(glm::radians(planet.angle)));
            planet.body.position = planetPos;

            if (checkCollision(cameraPos, 0.5f, planet.body.position, planet.body.collisionRadius) ||
                checkCollision(cameraPos, 0.5f, sun.position, sun.collisionRadius)) {
                gameOver = true;
            }

            model = glm::mat4(1.0f);
            model = glm::translate(model, planetPos);
            model = glm::rotate(model, glm::radians(planet.rotation), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(planet.size));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, planet.textureID);
            glUniform1i(glGetUniformLocation(planetShaderProgram, "planetTexture"), 0);
            glUniformMatrix4fv(glGetUniformLocation(planetShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(glGetUniformLocation(planetShaderProgram, "objectColor"), planet.color.r, planet.color.g, planet.color.b);
            glUniform1f(glGetUniformLocation(planetShaderProgram, "darkness"), planet.darkness);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        if (flashlightOn) { renderText("Flashlight: ON", 10, 30, 0.5f); } //kiiras konzolra az egyszerubb kezelesert
        else { renderText("Flashlight: OFF - Press Key F", 10, 30, 0.5f); }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //tisztitas, torles a vegen
    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(planetShaderProgram);
    glDeleteProgram(sunShaderProgram);
    glDeleteProgram(skyboxShaderProgram);
    glfwTerminate();

    return 0;
}