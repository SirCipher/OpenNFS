//
// Created by Amrik on 25/10/2017.
//
#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include <utility>
#include <vector>
#include <cstdlib>
#include <string>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <LinearMath/btDefaultMotionState.h>

class Model {
public:
    Model(std::string name, int model_id, std::vector<glm::vec3> verts, std::vector<glm::vec2> uvs, std::vector<glm::vec3> norms,
          std::vector<unsigned int> indices, bool removeVertexIndexing, glm::vec3 center_position);
    std::string m_name;
    std::vector<glm::vec3> m_vertices;
    std::vector<glm::vec3> m_normals;
    std::vector<glm::vec2> m_uvs;
    std::vector<unsigned int> m_vertex_indices;

    void enable();

    virtual bool genBuffers()= 0;

    virtual void update()= 0;

    virtual void destroy()= 0;

    virtual void render()= 0;

    int id;
    /*--------- Model State --------*/
    //UI
    bool enabled = false;
    //Rendering
    glm::mat4 ModelMatrix = glm::mat4(1.0);
    glm::mat4 RotationMatrix;
    glm::mat4 TranslationMatrix;
    glm::vec3 position;
    glm::vec3 orientation_vec;
    glm::quat orientation;


    /* Iterators to allow for ranged for loops with class*/
    class iterator {
    public:
        explicit iterator(Model *ptr) : ptr(ptr) { }
        iterator operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const iterator &other) { return ptr != other.ptr; }
        const Model &operator*() const { return *ptr; }
    private:
        Model *ptr;
    };
    iterator begin() const { return iterator(val); }
    iterator end() const { return iterator(val + len); }
    Model *val;
protected:
    GLuint VertexArrayID;
private:
    /* Iterator vars */
    unsigned len;
};