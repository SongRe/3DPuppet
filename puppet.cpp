

#include "puppet.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

static bool show_gui = true;
static bool do_picking = false;

const size_t CIRCLE_PTS = 48;
// forward decl
glm::vec3 mapToSphere(float x, float y, float diameter);
//----------------------------------------------------------------------------------------
// Constructor
Puppet::Puppet(const std::string & luaSceneFile)
	: m_luaSceneFile(luaSceneFile),
	  m_positionAttribLocation(0),
	  m_normalAttribLocation(0),
	  m_vao_meshData(0),
	  m_vbo_vertexPositions(0),
	  m_vbo_vertexNormals(0),
	  m_vao_arcCircle(0),
	  m_vbo_arcCircle(0),
	  interactionMode(InteractionMode::POSITION),
	  option_circle(false),
      option_zbuffer(true),  
      option_backface(false),
      option_frontface(false)
{

}

//----------------------------------------------------------------------------------------
// Destructor
Puppet::~Puppet()
{
	idToInitialNodeInfo.clear();
}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void Puppet::init()
{
	// Set the background colour.
	glClearColor(0.85, 0.85, 0.85, 1.0);

	createShaderProgram();

	glGenVertexArrays(1, &m_vao_arcCircle);
	glGenVertexArrays(1, &m_vao_meshData);
	enableVertexShaderInputSlots();

	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
			getAssetFilePath("cube.obj"),
			getAssetFilePath("sphere.obj"),
			getAssetFilePath("suzanne.obj")
	});


	// Acquire the BatchInfoMap from the MeshConsolidator.
	meshConsolidator->getBatchInfoMap(m_batchInfoMap);

	// Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
	uploadVertexDataToVbos(*meshConsolidator);

	mapVboDataToVertexShaderInputLocations();

	initPerspectiveMatrix();

	initViewMatrix();

	initLightSources();

	initNodeInfo(m_rootNode.get());

	resetAll();


	// Exiting the current scope calls delete automatically on meshConsolidator freeing
	// all vertex data resources.  This is fine since we already copied this data to
	// VBOs on the GPU.  We have no use for storing vertex data on the CPU side beyond
	// this point.
}

//----------------------------------------------------------------------------------------
void Puppet::processLuaSceneFile(const std::string & filename) {
	// This version of the code treats the Lua file as an Asset,
	// so that you'd launch the program with just the filename
	// of a puppet in the Assets/ directory.
	// std::string assetFilePath = getAssetFilePath(filename.c_str());
	// m_rootNode = std::shared_ptr<SceneNode>(import_lua(assetFilePath));

	// This version of the code treats the main program argument
	// as a straightforward pathname.
	m_rootNode = std::shared_ptr<SceneNode>(import_lua(filename));
	if (!m_rootNode) {
		std::cerr << "Could Not Open " << filename << std::endl;
	}
}

//----------------------------------------------------------------------------------------
void Puppet::createShaderProgram()
{
	m_shader.generateProgramObject();
	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
	m_shader.link();

	m_shader_arcCircle.generateProgramObject();
	m_shader_arcCircle.attachVertexShader( getAssetFilePath("arc_VertexShader.vs").c_str() );
	m_shader_arcCircle.attachFragmentShader( getAssetFilePath("arc_FragmentShader.fs").c_str() );
	m_shader_arcCircle.link();
}

//----------------------------------------------------------------------------------------
void Puppet::enableVertexShaderInputSlots()
{
	//-- Enable input slots for m_vao_meshData:
	{
		glBindVertexArray(m_vao_meshData);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_positionAttribLocation = m_shader.getAttribLocation("position");
		glEnableVertexAttribArray(m_positionAttribLocation);

		// Enable the vertex shader attribute location for "normal" when rendering.
		m_normalAttribLocation = m_shader.getAttribLocation("normal");
		glEnableVertexAttribArray(m_normalAttribLocation);

		CHECK_GL_ERRORS;
	}


	//-- Enable input slots for m_vao_arcCircle:
	{
		glBindVertexArray(m_vao_arcCircle);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_arc_positionAttribLocation = m_shader_arcCircle.getAttribLocation("position");
		glEnableVertexAttribArray(m_arc_positionAttribLocation);

		CHECK_GL_ERRORS;
	}

	// Restore defaults
	glBindVertexArray(0);
}

//----------------------------------------------------------------------------------------
void Puppet::uploadVertexDataToVbos (
		const MeshConsolidator & meshConsolidator
) {
	// Generate VBO to store all vertex position data
	{
		glGenBuffers(1, &m_vbo_vertexPositions);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
				meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store all vertex normal data
	{
		glGenBuffers(1, &m_vbo_vertexNormals);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexNormalBytes(),
				meshConsolidator.getVertexNormalDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store the trackball circle.
	{
		glGenBuffers( 1, &m_vbo_arcCircle );
		glBindBuffer( GL_ARRAY_BUFFER, m_vbo_arcCircle );

		float *pts = new float[ 2 * CIRCLE_PTS ];
		for( size_t idx = 0; idx < CIRCLE_PTS; ++idx ) {
			float ang = 2.0 * M_PI * float(idx) / CIRCLE_PTS;
			pts[2*idx] = cos( ang );
			pts[2*idx+1] = sin( ang );
		}

		glBufferData(GL_ARRAY_BUFFER, 2*CIRCLE_PTS*sizeof(float), pts, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}
}

//----------------------------------------------------------------------------------------
void Puppet::mapVboDataToVertexShaderInputLocations()
{
	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_meshData);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
	glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
	// "normal" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
	glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;

	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_arcCircle);

	// Tell GL how to map data from the vertex buffer "m_vbo_arcCircle" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_arcCircle);
	glVertexAttribPointer(m_arc_positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Puppet::initPerspectiveMatrix()
{
	float aspect = ((float)m_windowWidth) / m_windowHeight;
	m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
}


//----------------------------------------------------------------------------------------
void Puppet::initViewMatrix() {
	m_view = glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f),
			vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void Puppet::initLightSources() {
	// World-space position
	m_light.position = vec3(10.0f, 10.0f, 10.0f);
	m_light.rgbIntensity = vec3(1.0f); // light
}


void Puppet::initNodeInfo(SceneNode* node) {
	if (node == nullptr) {
		return;
	}
	int id = node->m_nodeId;
	if (idToInitialNodeInfo.find(id) == idToInitialNodeInfo.end()) {
		idToInitialNodeInfo[id] = new NodeInfo(node);
	}

	for (auto child : node->children) {
		initNodeInfo(child);
	}
}

//----------------------------------------------------------------------------------------
void Puppet::uploadCommonSceneUniforms() {
	m_shader.enable();
	{
		//-- Set Perpsective matrix uniform for the scene:
		GLint location = m_shader.getUniformLocation("Perspective");
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
		CHECK_GL_ERRORS;

		// Decide whether or not rendering in picking mode
		location = m_shader.getUniformLocation("picking");
		glUniform1i(location, do_picking ? 1 : 0);
		
		if (!do_picking) {
			//-- Set LightSource uniform for the scene:
			{
				location = m_shader.getUniformLocation("light.position");
				glUniform3fv(location, 1, value_ptr(m_light.position));
				location = m_shader.getUniformLocation("light.rgbIntensity");
				glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
				CHECK_GL_ERRORS;
			}

			//-- Set background light ambient intensity
			{
				location = m_shader.getUniformLocation("ambientIntensity");
				vec3 ambientIntensity(0.25f);
				glUniform3fv(location, 1, value_ptr(ambientIntensity));
				CHECK_GL_ERRORS;
			}
		}
		
	}
	m_shader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Puppet::appLogic()
{
	// Place per frame, application logic here ...

	uploadCommonSceneUniforms();

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void Puppet::guiLogic()
{
	if( !show_gui ) {
		return;
	}

	static bool firstRun(true);
	if (firstRun) {
		ImGui::SetNextWindowPos(ImVec2(50, 50));
		firstRun = false;
	}

	static bool showDebugWindow(true);
	ImGuiWindowFlags windowFlags(ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_AlwaysAutoResize);
	float opacity(0.5f);


    if (ImGui::Begin("Properties", &showDebugWindow, ImVec2(100,100), opacity, windowFlags)) {
		// Create the main menu bar at the top of the window.
		if( ImGui::BeginMenuBar() ) {
			// Application Menu
			if( ImGui::BeginMenu("Application") ) {
				if( ImGui::MenuItem("Reset Position (I)") ) {
					resetPosition();
				}
				if( ImGui::MenuItem("Reset Orientation (O)") ) {
					resetOrientation();
				}
				if( ImGui::MenuItem("Reset Joints (S)") ) {
					resetJoints();
				}

				if( ImGui::MenuItem("Reset All (A)") ) {
					resetAll();
				}
				if( ImGui::MenuItem("Quit (Q)") ) {
					glfwSetWindowShouldClose(m_window, GL_TRUE);
				}
				ImGui::EndMenu();
			}

			// Edit Menu
			if( ImGui::BeginMenu("Edit") ) {
				if( ImGui::MenuItem("Undo (U)") ) {
					undo();
				}
				if( ImGui::MenuItem("Redo (R)") ) {
					redo();
				}
				ImGui::EndMenu();
			}

			// Options Menu
			if( ImGui::BeginMenu("Options") ) {

				ImGui::MenuItem("Circle (C)", NULL, &option_circle);
				ImGui::MenuItem("Z-buffer (Z)", NULL, &option_zbuffer);
				ImGui::MenuItem("Backface Culling (B)", NULL, &option_backface);
				ImGui::MenuItem("Frontface Culling (F)", NULL, &option_frontface);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
		if (ImGui::RadioButton("Position/Orientation (P)", reinterpret_cast<int*>(&interactionMode), InteractionMode::POSITION)) {
			handleInteractionMode();
		}
		if (ImGui::RadioButton("Joints (J)", reinterpret_cast<int*>(&interactionMode), InteractionMode::JOINT)) {
			handleInteractionMode();
		}
		ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();
	}

}

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
static void updateShaderUniforms(
		const ShaderProgram & shader,
		const GeometryNode & node,
		const glm::mat4 & viewMatrix
) {

	shader.enable();
	{
		//-- Set ModelView matrix:
		GLint location = shader.getUniformLocation("ModelView");
		mat4 modelView = viewMatrix;
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(modelView));
		CHECK_GL_ERRORS;
		if ( do_picking ) {
			// unique
			GLint location = shader.getUniformLocation("material.kd");
			float r = float(node.m_nodeId & 0xff) / 255.0f;
			float g = float((node.m_nodeId >> 8) & 0xff) / 255.0f;
			float b = float((node.m_nodeId >> 16) & 0xff) / 255.0f;
			glUniform3f(location, r, g, b);
			CHECK_GL_ERRORS;
		} else {
			//-- Set NormMatrix:
			location = shader.getUniformLocation("NormalMatrix");
			mat3 normalMatrix = glm::transpose(glm::inverse(mat3(modelView)));
			glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
			CHECK_GL_ERRORS;

			//-- Set Material values:
			location = shader.getUniformLocation("material.kd");
			vec3 kd = node.material.kd;
			if (node.isSelected) {
				kd = vec3(1.0f, 1.0f, 0.0f);
			}
			glUniform3fv(location, 1, value_ptr(kd));
			CHECK_GL_ERRORS;
			location = shader.getUniformLocation("material.ks");
			vec3 ks = node.material.ks;
			glUniform3fv(location, 1, value_ptr(ks));
			CHECK_GL_ERRORS;
			location = shader.getUniformLocation("material.shininess");
			glUniform1f(location, node.material.shininess);
			CHECK_GL_ERRORS;
		}
		
	}
	shader.disable();

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Puppet::draw() {

	if (option_zbuffer) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
	handleCulling();
	renderSceneGraph(*m_rootNode);


	glDisable( GL_DEPTH_TEST );
	if (option_circle) {
		renderArcCircle();
	}
}



void Puppet::renderSceneNode(const SceneNode *node, const glm::mat4 transform) {
	if (node == nullptr) {
		return;
	}
	glm::mat4 currentTransform = transform * node->get_transform();
	// 
	// cout << "current node: " << *node << endl;
	// cout << "current transform: " << currentTransform << endl;
	// cout << "current node transform" << node->get_transform() << endl;
	// If the node is a GeometryNode, render its geometry.
    if (node->m_nodeType == NodeType::GeometryNode) {
        const GeometryNode * geometryNode = static_cast<const GeometryNode *>(node);
        // Update the shader uniforms using the accumulated transform.

        updateShaderUniforms(m_shader, *geometryNode, currentTransform);

        // Retrieve the batch info for this geometry.
        BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

        m_shader.enable();
        glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
        m_shader.disable();
    }

	// dfs to render all children.
	// assume no loops
    for (const SceneNode* child : node->children) {
        renderSceneNode(child, currentTransform);
    }
}

//----------------------------------------------------------------------------------------
void Puppet::renderSceneGraph(const SceneNode & root) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
	glBindVertexArray(m_vao_meshData);

	// This is emphatically *not* how you should be drawing the scene graph in
	// your final implementation.  This is a non-hierarchical demonstration
	// in which we assume that there is a list of GeometryNodes living directly
	// underneath the root node, and that we can draw them in a loop.  It's
	// just enough to demonstrate how to get geometry and materials out of
	// a GeometryNode and onto the screen.

	// You'll want to turn this into recursive code that walks over the tree.
	// You can do that by putting a method in SceneNode, overridden in its
	// subclasses, that renders the subtree rooted at every node.  Or you
	// could put a set of mutually recursive functions in this class, which
	// walk down the tree from nodes of different types.


	// apply translation to view only and rotation to puppet only
	glm::mat4 transformedView = m_view * puppet_translation * puppet_transform * puppet_rotation;

	renderSceneNode(&root, transformedView);

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
// Draw the trackball circle.
void Puppet::renderArcCircle() {
	glBindVertexArray(m_vao_arcCircle);

	m_shader_arcCircle.enable();
		GLint m_location = m_shader_arcCircle.getUniformLocation( "M" );
		float aspect = float(m_framebufferWidth)/float(m_framebufferHeight);
		glm::mat4 M;
		if( aspect > 1.0 ) {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5/aspect, 0.5, 1.0 ) );
		} else {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5, 0.5*aspect, 1.0 ) );
		}
		glUniformMatrix4fv( m_location, 1, GL_FALSE, value_ptr( M ) );
		glDrawArrays( GL_LINE_LOOP, 0, CIRCLE_PTS );
	m_shader_arcCircle.disable();

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

// =========================================== Helper Methods ========================================== //
void Puppet::handleCulling() {
	if (option_backface || option_frontface) {
        glEnable(GL_CULL_FACE);
        if (option_backface && option_frontface) {
            glCullFace(GL_FRONT_AND_BACK);
        } else if (option_backface) {
            glCullFace(GL_BACK);
        } else if (option_frontface) {
            glCullFace(GL_FRONT);
        }
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Puppet::applyPositionTransform(double xPos, double yPos) {
	double oldX = prev_mouse_x;
	double oldY = prev_mouse_y;

	float trackballDiameter = 0.5f * std::min(m_framebufferWidth, m_framebufferHeight);

	// calc delta
	double dx = xPos - oldX;
	double dy = yPos - oldY;

	

	// X / Y translations
	if (mouse_left_down) {
		glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(translationScale * (float)dx,
													-translationScale * (float)dy, 0.0f));
		// m_view = m_view * T;
		puppet_translation = T * puppet_translation;

	}
	// Z translation
	else if (mouse_middle_down) {
		glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, translationScale * (float)dy));
		puppet_translation = T * puppet_translation;
	}

	// trackball
	else if (mouse_right_down) {
		float width = static_cast<float>(m_windowWidth);
		float height = static_cast<float>(m_windowHeight);

		// get centered width and height
		float centerX = width / 2.0f;
		float centerY = height / 2.0f;

		// get coords relative to center
		float relOldX = oldX - centerX;
		float relOldY = oldY - centerY;
		float relNewX = xPos - centerX;
		float relNewY = yPos - centerY;


		glm::vec3 prevPos = mapToSphere(relOldX, relOldY, trackballDiameter);
		glm::vec3 currPos = mapToSphere(relNewX, relNewY, trackballDiameter);

		// rotation axis as the cross product.
		glm::vec3 rotAxis = glm::cross(prevPos, currPos);

		float axisLength = glm::length(rotAxis);
		// rotAxis.y = -rotAxis.y; // invert 
		
		if (axisLength > 1e-6f) {
			rotAxis = glm::normalize(rotAxis);
			float dotVal = glm::dot(prevPos, currPos);
			dotVal = glm::clamp(dotVal, -1.0f, 1.0f);
			float angle = acos(dotVal);
			
			// rotation matrix
			glm::mat4 R = glm::rotate(glm::mat4(1.0f), angle, rotAxis);

			// assume first child we will say is origin of the object
			glm::vec3 pivot;
			if (m_rootNode.get() && m_rootNode.get()->children.front() != nullptr) {
				glm::mat4 root_transform = m_rootNode.get()->get_transform();
				glm::mat4 first_child_local = m_rootNode.get()->children.front()->get_transform();
				glm::mat4 child_global = puppet_transform * root_transform * first_child_local;
				pivot = glm::vec3(child_global[3]);
			} else {
				pivot = glm::vec3(puppet_transform[3]);
			}	

			// build the translation matrices to move the pivot to the origin and back.
			glm::mat4 T1 = glm::translate(glm::mat4(1.0f), -pivot);
			glm::mat4 T2 = glm::translate(glm::mat4(1.0f),  pivot);
			glm::mat4 total_rotation = T2 * R * T1;

			puppet_rotation = total_rotation * puppet_rotation;
		}
	}
}

SceneNode* Puppet::findSceneNodeById(SceneNode* node, unsigned int id) {
	if (!node) {
		return nullptr;
	}
	// if the current node we're at isn't registered in the map
	// add it to the map
	if (idToSceneNode.find(node->m_nodeId) == idToSceneNode.end()) {
		idToSceneNode[node->m_nodeId] = node;
	}

	// if the node we're looking for is in the map
	if (idToSceneNode.find(id) != idToSceneNode.end()) {
		return idToSceneNode[id];
	}
	if (node->m_nodeId == id) {
		return node;
	}

	for (auto c : node->children) {
		SceneNode* result = findSceneNodeById(c, id);
		if (result != nullptr) {
			return result;
		}
	}
	return nullptr;
}

void Puppet::handleInteractionMode() {
	if (interactionMode == JOINT) {
		// m_light.rgbIntensity = vec3(0.0f);
	} else {
		// m_light.rgbIntensity = vec3(1.0f);
	}
}

void Puppet::applyJointTransform(double xPos, double yPos) {
	double dy = yPos - prev_mouse_y;
	float sensitivity = 0.2f;
	float deltaAngle = static_cast<float>(dy) * sensitivity;
	if (mouse_middle_down) {
		for (SceneNode* node : selected_nodes) {
			if (node->m_nodeType == NodeType::JointNode) {
				JointNode* joint = static_cast<JointNode*>(node);
				// we will be using x mins and maxes for rotations on z
				double new_angle = joint->current_angle_z + deltaAngle;
				double x_min = joint->m_joint_x.min;
				double x_max = joint->m_joint_x.max;
				if (new_angle > x_max || new_angle < x_min) {
					continue;
				}
				node->rotate('z', deltaAngle);
			}
		}
	}
	if (mouse_right_down) {
		// hard coded 4 -> head
		SceneNode *head = idToSceneNode[4];
		SceneNode* head_parent = head->m_parent;
		if (head_parent->m_nodeType == NodeType::JointNode && head->isSelected) {
			JointNode* joint = static_cast<JointNode*>(head_parent);
			double new_angle = joint->current_angle_y + deltaAngle;
			double y_min = joint->m_joint_y.min;
			double y_max = joint->m_joint_y.max;
			if (new_angle > y_max || new_angle < y_min) {
				return;
			}
			head_parent->rotate('y', deltaAngle);
		}
	}
	
}

void Puppet::pickingSetup() {
	do_picking = true;
	uploadCommonSceneUniforms(); // Make sure the shader gets do_picking = true.

	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.35, 0.35, 0.35, 1.0);

	draw();

	CHECK_GL_ERRORS;

	double xpos, ypos;
	glfwGetCursorPos(m_window, &xpos, &ypos);

	xpos *= double(m_framebufferWidth) / double(m_windowWidth);
	ypos = m_windowHeight - ypos;
	ypos *= double(m_framebufferHeight) / double(m_windowHeight);

	GLubyte buffer[ 4 ] = { 0, 0, 0, 0 };
	// A bit ugly -- don't want to swap the just-drawn false colours
	// to the screen, so read from the back buffer.
	glReadBuffer( GL_BACK );
	// Actually read the pixel at the mouse location.
	glReadPixels( int(xpos), int(ypos), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
	CHECK_GL_ERRORS;

	// Reassemble the object ID.
	unsigned int pickedID = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16);

	SceneNode* pickedNode = findSceneNodeById(m_rootNode.get(), pickedID);
	if (pickedNode && pickedNode->m_nodeType == NodeType::GeometryNode) {
		// Toggle the selection flag
		pickedNode->isSelected = !pickedNode->isSelected;
		// cout << "picked node id: " << pickedNode->m_nodeId << endl;
		SceneNode* parent = pickedNode->m_parent;
		// Sync parent (joint node)
		if (parent && parent->m_nodeType == NodeType::JointNode) {
			parent->isSelected = pickedNode->isSelected;
			if (parent->isSelected) {
				selected_nodes.insert(parent);
			} else {
				selected_nodes.erase(parent);
			}
		} 
	}
	do_picking = false;
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void Puppet::cleanup()
{

}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool Puppet::cursorEnterWindowEvent (
		int entered
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool Puppet::mouseMoveEvent (
		double xPos,
		double yPos
) {
	bool eventHandled(false);

	if (interactionMode == InteractionMode::POSITION) {
		eventHandled = true;
		applyPositionTransform(xPos, yPos);
	} else if (interactionMode == InteractionMode::JOINT) {
		eventHandled = true;
		applyJointTransform(xPos, yPos);
	}
	prev_mouse_x = xPos;
	prev_mouse_y = yPos;
	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool Puppet::mouseButtonInputEvent (
		int button,
		int actions,
		int mods
) {
	bool eventHandled(false);
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		if (actions == GLFW_PRESS) {
			mouse_dragging = true;
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				mouse_left_down = true;
				if (interactionMode == InteractionMode::JOINT) {
					pickingSetup();
				}
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				mouse_middle_down = true;
				if (interactionMode == InteractionMode::JOINT) {
					// on push, we want to save the current state of selected nodes
					saveState();
				}
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				mouse_right_down = true;
				if (interactionMode == InteractionMode::JOINT) {
					saveState();
				}
				eventHandled = true;
			}
			// Record the initial mouse position.
			glfwGetCursorPos(m_window, &prev_mouse_x, &prev_mouse_y);
		}
		else if (actions == GLFW_RELEASE) {
			mouse_dragging = false;
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				mouse_left_down = false;
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				mouse_middle_down = false;

				// on release, we should push the last state to the undo stack
				undo_stack.push(cur_node_info);
				redo_stack = stack<vector<NodeInfo>>();

				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				mouse_right_down = false;
				// 
				undo_stack.push(cur_node_info);
				redo_stack = stack<vector<NodeInfo>>();

				eventHandled = true;
			}
		}
	}
	
	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool Puppet::mouseScrollEvent (
		double xOffSet,
		double yOffSet
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool Puppet::windowResizeEvent (
		int width,
		int height
) {
	bool eventHandled(false);
	initPerspectiveMatrix();
	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool Puppet::keyInputEvent (
		int key,
		int action,
		int mods
) {
	bool eventHandled(false);

	if( action == GLFW_PRESS ) {
        switch(key) {
            case GLFW_KEY_I:
                resetPosition();
                eventHandled = true;
                break;
            case GLFW_KEY_O:
                resetOrientation();
                eventHandled = true;
                break;
            case GLFW_KEY_S:
                resetJoints();
                eventHandled = true;
                break;
            case GLFW_KEY_A:
                resetAll();
                eventHandled = true;
                break;
            case GLFW_KEY_Q:
                glfwSetWindowShouldClose(m_window, GL_TRUE);
                eventHandled = true;
                break;
            case GLFW_KEY_U:
                undo();
                eventHandled = true;
                break;
            case GLFW_KEY_R:
                redo();
                eventHandled = true;
                break;
            case GLFW_KEY_M:
                show_gui = !show_gui;
                eventHandled = true;
                break;
            case GLFW_KEY_P:
                // Emulate clicking the "Position/Orientation" radio button.
                interactionMode = InteractionMode::POSITION;
				handleInteractionMode();
                eventHandled = true;
                break;
            case GLFW_KEY_J:
                // Emulate clicking the "Joints" radio button.
                interactionMode = InteractionMode::JOINT;
				handleInteractionMode();
                eventHandled = true;
                break;
			case GLFW_KEY_C:
                // Toggle the trackball circle display.
                option_circle = !option_circle;
                eventHandled = true;
                break;
            case GLFW_KEY_Z:
                option_zbuffer = !option_zbuffer;
                eventHandled = true;
                break;
            case GLFW_KEY_B:
                option_backface = !option_backface;
                eventHandled = true;
                break;
            case GLFW_KEY_F:
                option_frontface = !option_frontface;
                eventHandled = true;
                break;
            default:
                break;
        }
    }

	return eventHandled;
}



// UI Methods
void Puppet::resetPosition() {
	puppet_translation = mat4(1.0f);
	m_view = m_view * puppet_translation;
}

void Puppet::resetOrientation() {
	puppet_rotation = mat4(1.0f);
	puppet_transform = puppet_rotation * puppet_transform;
}

void Puppet::resetJoints() {
    // TODO: clear the undo/redo stack.


	// reset selected_nodes set
	// reset joint transforms
	queue<SceneNode*> q;
	q.push(m_rootNode.get());
	while (!q.empty()) {
		SceneNode* curNode = q.front();
		q.pop();
		if (selected_nodes.find(curNode) != selected_nodes.end()) {
			curNode->isSelected = false;
			selected_nodes.erase(curNode);
		}
		// apply the initial node info
		idToInitialNodeInfo[curNode->m_nodeId]->apply();
		for (auto c : curNode->children) {
			q.push(c);
		}
	}
}

void Puppet::resetAll() {
    // TODO: Reset position, orientation, joints, and clear the undo/redo stack.
	interactionMode = InteractionMode::POSITION;
	option_circle = false;
	option_zbuffer = true;
	option_backface = false;
	option_frontface = false;

	// Initialize mouse state
	mouse_dragging = false;
	mouse_left_down = false;
	mouse_middle_down = false;
	mouse_right_down = false;
	prev_mouse_x = 0.0;
	prev_mouse_y = 0.0;

	resetOrientation();
	resetPosition();
	resetJoints();

	do_picking = false;

}

// save current joint states
void Puppet::saveState() {
	cur_node_info.clear();

	// save currently selected node states
	for (auto node : selected_nodes) {
		NodeInfo copy(node);
		cur_node_info.push_back(copy);
	}
}

void Puppet::undo() {
	if (undo_stack.empty()) return;
	vector<NodeInfo> undo_info = undo_stack.top();
	undo_stack.pop();
	saveState(); // save current state before undo
	redo_stack.push(cur_node_info);
	for (auto info : undo_info) {
		info.apply();
	}
}

void Puppet::redo() {
	if (redo_stack.empty()) return;
	vector<NodeInfo> redo_info = redo_stack.top();
	redo_stack.pop();
	saveState(); // save current state before undo
	undo_stack.push(cur_node_info);
	for (auto info : redo_info) {
		info.apply();
	}
}




// =========================================== TRACKBALL ===================================================

// map 2D coordinates (in pixels, relative to the circle's center) to 3D point on sphere. 
// clamp z coordinate to 0 if outside sphere. 
glm::vec3 mapToSphere(float x, float y, float diameter) {
    float newX = x * 2.0f / diameter;
    float newY = y * 2.0f / diameter;

    float sq = 1.0f - newX * newX - newY * newY;

	// outside sphere, clamp Z to 0
    if (sq < 0.0f) {
        float length = sqrt(newX * newX + newY * newY);
        newX /= length;
        newY /= length;
        return glm::vec3(newX, newY, 0.0f);
    } else {
        return glm::vec3(newX, newY, sqrt(sq));
    }
}
