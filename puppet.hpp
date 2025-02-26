// Termm-Fall 2020

#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SceneNode.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <bits/stdc++.h>

struct LightSource {
	glm::vec3 position;
	glm::vec3 rgbIntensity;
};

enum InteractionMode {
	POSITION,
	JOINT
};

// for undo redo
struct NodeInfo {
	SceneNode* node;
	double cur_angle_y;
	double cur_angle_z;
	bool isSelected;
	glm::mat4 transform;

	NodeInfo(SceneNode* node) {
		this->node = node;
		cur_angle_y = node->current_angle_y;
		cur_angle_z = node->current_angle_z;
		isSelected = node->isSelected;
		transform = node->get_transform();
	}

	void apply() {
		node->current_angle_z = cur_angle_z;
		node->current_angle_y = cur_angle_y;
		node->isSelected = isSelected;
		node->set_transform(transform);
	}
};

class Puppet : public CS488Window {
public:
	Puppet(const std::string & luaSceneFile);
	virtual ~Puppet();

	const float translationScale = 0.01f;

protected:
	virtual void init() override;
	virtual void appLogic() override;
	virtual void guiLogic() override;
	virtual void draw() override;
	virtual void cleanup() override;

	// UI Methods
	void resetPosition();
    void resetOrientation();
    void resetJoints();
    void resetAll();

	// undo redo
	void saveState();
    void undo();
    void redo();

	// data for undo/redo
	std::vector<NodeInfo> cur_node_info;
	std::stack<std::vector<NodeInfo>> redo_stack;
	std::stack<std::vector<NodeInfo>> undo_stack;

	//-- Virtual callback methods
	virtual bool cursorEnterWindowEvent(int entered) override;
	virtual bool mouseMoveEvent(double xPos, double yPos) override;
	virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
	virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
	virtual bool windowResizeEvent(int width, int height) override;
	virtual bool keyInputEvent(int key, int action, int mods) override;

	//-- One time initialization methods:
	void processLuaSceneFile(const std::string & filename);
	void createShaderProgram();
	void enableVertexShaderInputSlots();
	void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
	void mapVboDataToVertexShaderInputLocations();
	void initViewMatrix();
	void initLightSources();
	void initNodeInfo(SceneNode *node);

	void initPerspectiveMatrix();
	void uploadCommonSceneUniforms();
	void renderSceneGraph(const SceneNode &node);
	void renderSceneNode(const SceneNode* node, const glm::mat4 parentTransform);
	void renderArcCircle();

	// Helper methods
	void handleCulling();
	void applyPositionTransform(double xPos, double yPos);
	SceneNode* findSceneNodeById(SceneNode* node, unsigned int id);
	void handleInteractionMode();
	void applyJointTransform(double xPos, double yPos);
	void pickingSetup();

	std::unordered_set<SceneNode*> selected_nodes;
	std::unordered_map<int, SceneNode*> idToSceneNode;
	std::unordered_map<int, NodeInfo*> idToInitialNodeInfo;

	glm::mat4 m_perpsective;
	glm::mat4 m_view;

	LightSource m_light;

	//-- GL resources for mesh geometry data:
	GLuint m_vao_meshData;
	GLuint m_vbo_vertexPositions;
	GLuint m_vbo_vertexNormals;
	GLint m_positionAttribLocation;
	GLint m_normalAttribLocation;
	ShaderProgram m_shader;

	//-- GL resources for trackball circle geometry:
	GLuint m_vbo_arcCircle;
	GLuint m_vao_arcCircle;
	GLint m_arc_positionAttribLocation;
	ShaderProgram m_shader_arcCircle;

	// BatchInfoMap is an associative container that maps a unique MeshId to a BatchInfo
	// object. Each BatchInfo object contains an index offset and the number of indices
	// required to render the mesh with identifier MeshId.
	BatchInfoMap m_batchInfoMap;

	std::string m_luaSceneFile;

	std::shared_ptr<SceneNode> m_rootNode;

	// UI State
	bool option_circle = false;
	bool option_zbuffer = true;      // Default enabled.
	bool option_backface = false;
	bool option_frontface = false;
	InteractionMode interactionMode = POSITION;

	// Global transform (entire scene graph)
	glm::mat4 puppet_translation;
	glm::mat4 puppet_rotation;
    glm::mat4 puppet_transform;

    // Mouse drag state 
    bool mouse_dragging;
	bool mouse_left_down, mouse_middle_down, mouse_right_down;
    double prev_mouse_x, prev_mouse_y;


};
