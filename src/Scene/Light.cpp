//
// Created by Amrik on 01/03/2018.
//


#include "Light.h"
#include "../Util/Utils.h"

Light::Light(glm::vec3 light_position, glm::vec3 light_colour) : super("Light", 0, std::vector<glm::vec3>(), std::vector<glm::vec2>(), std::vector<glm::vec3>(), std::vector<unsigned int>(), false, light_position) {
    // TODO: Redo all of this to make sense
    std::vector<glm::vec3> verts;
    verts.push_back(glm::vec3(-0.5, -0.5, 0)); // bottom left corner
    verts.push_back(glm::vec3(-0.5,  0.5, 0)); // top left corner
    verts.push_back(glm::vec3( 0.5,  0.5, 0)); // top right corner
    verts.push_back(glm::vec3( 0.5, -0.5, 0)); // bottom right corner
    unsigned int indices[] = {0,1,2, // first triangle (bottom left - top left - top right)
                              0,2,3}; // second triangle (bottom left - top right - bottom right)
    m_uvs.clear();
    m_uvs.push_back(glm::vec2(0.0f, 1.0f));
    m_uvs.push_back(glm::vec2(1.0f, 0.0f));
    m_uvs.push_back(glm::vec2(1.0f, 1.0f));
    m_uvs.push_back(glm::vec2(0.0f, 1.0f));
    m_uvs.push_back(glm::vec2(1.0f, 1.0f));
    m_uvs.push_back(glm::vec2(0.0f, 1.0f));

    m_vertex_indices = std::vector<unsigned int>(indices, indices + sizeof(indices)/sizeof(indices[0]));;
    m_vertices.clear();
    for (unsigned int m_vertex_index : m_vertex_indices) {
        m_vertices.push_back(verts[m_vertex_index]);
    }
    position= light_position;
    type = 1;
    colour = light_colour;

    ASSERT(genBuffers(), "Unable to generate GL Buffers for Light");
}

Light::Light(INTPT light_position, uint32_t light_type) : super("Light", 0, std::vector<glm::vec3>(), std::vector<glm::vec2>(), std::vector<glm::vec3>(), std::vector<unsigned int>(), false, glm::vec3()) {
    // TODO: Redo all of this to make sense
    std::vector<glm::vec3> verts;
    verts.push_back(glm::vec3(0, -0.5, -0.5)); // bottom left corner
    verts.push_back(glm::vec3(0,  0.5, -0.5)); // top left corner
    verts.push_back(glm::vec3(0,  0.5,  0.5)); // top right corner
    verts.push_back(glm::vec3(0, -0.5,  0.5)); // bottom right corner
    unsigned int indices[] = {0,1,2, // first triangle (bottom left - top left - top right)
                              0,2,3}; // second triangle (bottom left - top right - bottom right)

    m_uvs.clear();

    m_uvs.push_back(glm::vec2(0.0f, 0.0f));
    m_uvs.push_back(glm::vec2(0.0f, 1.0f));
    m_uvs.push_back(glm::vec2(1.0f, 1.0f));

    m_uvs.push_back(glm::vec2(0.0f, 0.0f));
    m_uvs.push_back(glm::vec2(1.0f, 1.0f));
    m_uvs.push_back(glm::vec2(1.0f, 0.0f));


    m_vertex_indices = std::vector<unsigned int>(indices, indices + sizeof(indices)/sizeof(indices[0]));;
    m_vertices.clear();

    position.x = static_cast<float>(light_position.x / 65536.0)/10;
    position.y = static_cast<float>(light_position.y / 65536.0)/10;
    position.z = static_cast<float>(light_position.z / 65536.0)/10;

    for (unsigned int m_vertex_index : m_vertex_indices) {
        m_vertices.push_back(verts[m_vertex_index]);
    }

    type = light_type;
    colour = glm::vec3(0.8, 0.7,0.5);

    ASSERT(genBuffers(), "Unable to generate GL Buffers for Light");
};

void Light::update() {
    orientation_vec = glm::vec3(-SIMD_PI/2,0,0);
    orientation = glm::normalize(glm::quat(orientation_vec));
    RotationMatrix = glm::toMat4(orientation);
    //Rotate around center
    TranslationMatrix = glm::translate(glm::mat4(1.0), glm::vec3(0,0,0));
    ModelMatrix = TranslationMatrix * RotationMatrix;

    orientation_vec = glm::vec3(-SIMD_PI/2,0,0);
    orientation = glm::normalize(glm::quat(orientation_vec));
    RotationMatrix = glm::toMat4(orientation);
    // Move back to original position
    ModelMatrix *= glm::translate(glm::mat4(1.0), position) * RotationMatrix;
}

void Light::destroy() {
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteBuffers(1, &uvbuffer);
    glDeleteBuffers(1, &normalBuffer);
}

void Light::render() {
    if (enabled){
        glBindVertexArray(VertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());
        glBindVertexArray(0);
    }
}

bool Light::genBuffers() {
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    // Verts
    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(glm::vec3), &m_vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(
            0,                  // attribute
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void *) 0            // array buffer offset
    );
    // 1st attribute buffer : Vertices
    glEnableVertexAttribArray(0);
    // UVs
    glGenBuffers(1, &uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, m_uvs.size() * sizeof(glm::vec2), &m_uvs[0], GL_STATIC_DRAW);
    glVertexAttribPointer(
            1,                                // attribute
            2,                                // size
            GL_FLOAT,                         // type
            GL_FALSE,                         // normalized?
            0,                                // stride
            (void *) 0                          // array buffer offset
    );
    // 2nd attribute buffer : UVs
    glEnableVertexAttribArray(1);
    // Normals
    glGenBuffers(1, &normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size() * sizeof(glm::vec3), &m_normals[0], GL_STATIC_DRAW);
    glVertexAttribPointer(
            4,                  // attribute
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void *) 0            // array buffer offset
    );
    // 2nd attribute buffer : Board Normals
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return true;
}

