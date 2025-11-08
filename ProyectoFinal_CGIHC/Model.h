#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SOIL2/SOIL2.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"

using namespace std;

GLint TextureFromFile(const char* path, string directory);
GLuint createSolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

// ===================
//  Clase Model
// ===================
class Model
{
public:
    // Constructor, espera la ruta al modelo
    Model(GLchar* path)
    {
        this->loadModel(path);
    }

    // Dibuja el modelo (todas sus mallas)
    void Draw(Shader shader)
    {
        for (GLuint i = 0; i < this->meshes.size(); i++)
            this->meshes[i].Draw(shader);
    }

private:
    // Datos
    vector<Mesh> meshes;
    string directory;
    // Cache de texturas ya cargadas
    vector<Texture> textures_loaded;

    // Carga el modelo con ASSIMP
    void loadModel(string path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs);

        if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }

        // Carpeta del archivo
        this->directory = path.substr(0, path.find_last_of('/'));

        // Procesar recursivo
        this->processNode(scene->mRootNode, scene);
    }

    // Recorre nodos y procesa mallas
    void processNode(aiNode* node, const aiScene* scene)
    {
        for (GLuint i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            this->meshes.push_back(this->processMesh(mesh, scene));
        }
        for (GLuint i = 0; i < node->mNumChildren; i++)
            this->processNode(node->mChildren[i], scene);
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene)
    {
        vector<Vertex>  vertices;
        vector<GLuint>  indices;
        vector<Texture> textures;

        // Vértices
        for (GLuint i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 v;

            // Posición
            v.x = mesh->mVertices[i].x;
            v.y = mesh->mVertices[i].y;
            v.z = mesh->mVertices[i].z;
            vertex.Position = v;

            // Normal
            v.x = mesh->mNormals[i].x;
            v.y = mesh->mNormals[i].y;
            v.z = mesh->mNormals[i].z;
            vertex.Normal = v;

            // UV
            if (mesh->mTextureCoords[0])
            {
                glm::vec2 uv;
                uv.x = mesh->mTextureCoords[0][i].x;
                uv.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = uv;
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        // Índices
        for (GLuint i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (GLuint j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // Materiales
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            // Difusas
            vector<Texture> diffuseMaps =
                this->loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

            // Especulares
            vector<Texture> specularMaps =
                this->loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

            // *** PARCHE BLANCO POR DEFECTO ***
            // Si el material no trae map_Kd (diffuse), empuja una textura 1x1 blanca
            if (diffuseMaps.empty())
            {
                Texture t;
                t.id   = createSolidTexture(234, 234, 234, 255); // ~Kd 0.915...
                t.type = "texture_diffuse";
                t.path = "__default_white__";
                textures.push_back(t);
                // NO lo ponemos en textures_loaded para distinguirlo de archivos reales
            }
        }

        return Mesh(vertices, indices, textures);
    }

    // Carga las texturas de un material segun el tipo
    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;

        for (GLuint i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);

            // Reusar si ya está cargada
            GLboolean skip = false;
            for (GLuint j = 0; j < textures_loaded.size(); j++)
            {
                if (textures_loaded[j].path == str)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }

            if (!skip)
            {
                Texture texture;
                texture.id   = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str;

                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }

        return textures;
    }
};

// ===================
//  Utilidades
// ===================

// Crea una textura 1x1 RGBA en GPU con un color sólido
inline GLuint createSolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GLuint id; glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    unsigned char px[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

GLint TextureFromFile(const char* path, string directory)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    GLuint textureID;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, channels = 0;
    unsigned char* image = SOIL_load_image(filename.c_str(), &width, &height, &channels, SOIL_LOAD_AUTO);

    if (!image || width <= 0 || height <= 0)
    {
        std::cerr << "SOIL: failed to load image '" << filename << "'. Using white fallback.\n";
        // Devuelve una textura blanca por defecto para no “pintar negro”
        return (GLint)createSolidTexture(234, 234, 234, 255);
    }

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    SOIL_free_image_data(image);

    return textureID;
}
