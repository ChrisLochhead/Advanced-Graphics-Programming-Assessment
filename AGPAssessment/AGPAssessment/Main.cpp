#if _DEBUG
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#endif

#include "rt3d.h"
#include "rt3dObjLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stack>



using namespace std;

#define DEG_TO_RADIAN 0.017453293

// Globals

GLuint cubeIndexCount = 0;
GLuint bunnyIndexCount = 0;
GLuint meshObjects[3];

//shaders
GLuint phongShaderProgram;
GLuint refractionShaderProgram;
GLuint textureProgram;
GLuint shaderProgram;
GLuint skyboxProgram;
GLuint toonShaderProgram;
GLuint reflectionMapProgram;
GLuint gouraudShaderProgram;

//camera rotation
GLfloat r = 0.0f;

// camera vectors
glm::vec3 eye(-2.0f, 1.0f, 8.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

//matrix stack
stack<glm::mat4> mvStack;

GLuint uniformIndex;

// TEXTURE STUFF
GLuint textures[3];
GLuint skybox[5];

rt3d::lightStruct light0 = {
	{ 0.4f, 0.4f, 0.4f, 1.0f }, // ambient
{ 1.0f, 1.0f, 1.0f, 1.0f }, // diffuse
{ 1.0f, 1.0f, 1.0f, 1.0f }, // specular
{ -5.0f, 2.0f, 2.0f, 1.0f }  // position
};
glm::vec4 lightPos(-5.0f, 2.0f, 2.0f, 1.0f); //light position

rt3d::materialStruct material0 = {
	{ 0.2f, 0.4f, 0.2f, 1.0f }, // ambient
{ 0.5f, 1.0f, 0.5f, 1.0f }, // diffuse
{ 0.0f, 0.1f, 0.0f, 1.0f }, // specular
2.0f  // shininess
};
rt3d::materialStruct material1 = {
	{ 0.4f, 0.4f, 1.0f, 1.0f }, // ambient
{ 0.8f, 0.8f, 1.0f, 1.0f }, // diffuse
{ 0.8f, 0.8f, 0.8f, 1.0f }, // specular
1.0f  // shininess
};

// light attenuation
float attConstant = 1.0f;
float attLinear = 0.0f;
float attQuadratic = 0.0f;

//shader controller variable
//1 for gouraud
//2 for phong
//3 for toon
//4 for envmap
//5 for refractmap

int shaderController = 1;


// Set up rendering context using SDL
SDL_Window * setupRC(SDL_GLContext &context) {
	SDL_Window * window;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) // Initialize video
		rt3d::exitFatalError("Unable to initialize SDL");

	// Request an OpenGL 3.0 context.

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);  // double buffering on
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // 8 bit alpha buffering
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // Turn on x4 multisampling anti-aliasing (MSAA)

													   // Create 800x600 window
	window = SDL_CreateWindow("SDL/GLM/OpenGL Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!window) // Check window was created OK
		rt3d::exitFatalError("Unable to create window");

	context = SDL_GL_CreateContext(window); // Create opengl context and attach to window
	SDL_GL_SetSwapInterval(1); // set swap buffers to sync with monitor's vertical refresh rate
	return window;
}

// A simple texture loading function to load from bitmap files
GLuint loadBitmap(const char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

							  // load file - using core SDL library
	SDL_Surface *tmpSurface;
	tmpSurface = SDL_LoadBMP(fname);
	if (!tmpSurface) {
		std::cout << "Error loading bitmap" << std::endl;
	}

	// bind texture and set parameters
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_PixelFormat *format = tmpSurface->format;

	GLuint externalFormat, internalFormat;
	if (format->Amask) {
		internalFormat = GL_RGBA;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGBA : GL_BGRA;
	}
	else {
		internalFormat = GL_RGB;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, tmpSurface->w, tmpSurface->h, 0,
		externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_FreeSurface(tmpSurface); // texture loaded, free the temporary buffer
	return texID;	// return value of texture ID
}


// A simple cubemap loading function
GLuint loadCubeMap(const char *fname[6], GLuint *texID)
{
	glGenTextures(1, texID); // generate texture ID
	GLenum sides[6] = { GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y };
	SDL_Surface *tmpSurface;

	glBindTexture(GL_TEXTURE_CUBE_MAP, *texID); // bind texture and set parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GLuint externalFormat;
	for (int i = 0; i<6; i++)
	{
		// load file - using core SDL library
		tmpSurface = SDL_LoadBMP(fname[i]);
		if (!tmpSurface)
		{
			std::cout << "Error loading bitmap" << std::endl;
			return *texID;
		}

		// skybox textures should not have alpha (assuming this is true!)
		SDL_PixelFormat *format = tmpSurface->format;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;

		glTexImage2D(sides[i], 0, GL_RGB, tmpSurface->w, tmpSurface->h, 0,
			externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
		// texture loaded, free the temporary buffer
		SDL_FreeSurface(tmpSurface);
	}
	return *texID;	// return value of texure ID, redundant really
}


void init(void) {


	phongShaderProgram = rt3d::initShaders("phong.vert", "phong.frag");
	rt3d::setLight(phongShaderProgram, light0);
	rt3d::setMaterial(phongShaderProgram, material0);

	gouraudShaderProgram = rt3d::initShaders("gouraud.vert", "simple.frag");
	rt3d::setLight(gouraudShaderProgram, light0);
	rt3d::setMaterial(gouraudShaderProgram, material0);

	shaderProgram = rt3d::initShaders("phongEnvMap.vert", "phongEnvMap.frag");
	rt3d::setLight(shaderProgram, light0);
	rt3d::setMaterial(shaderProgram, material0);
	// set light attenuation shader uniforms
	uniformIndex = glGetUniformLocation(shaderProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(shaderProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(shaderProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);

	refractionShaderProgram = rt3d::initShaders("phongRefractionMap.vert", "phongRefractionMap.frag");
	rt3d::setLight(shaderProgram, light0);
	rt3d::setMaterial(shaderProgram, material0);
	// set light attenuation shader uniforms
	uniformIndex = glGetUniformLocation(refractionShaderProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(refractionShaderProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(refractionShaderProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);




	toonShaderProgram = rt3d::initShaders("toon.vert", "toon.frag");
	rt3d::setLight(toonShaderProgram, light0);
	rt3d::setMaterial(toonShaderProgram, material0);
	uniformIndex = glGetUniformLocation(toonShaderProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(toonShaderProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(toonShaderProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);
	
	reflectionMapProgram = rt3d::initShaders("phongEnvMap.vert", "phongEnvMap.frag");
	rt3d::setLight(reflectionMapProgram, light0);
	rt3d::setMaterial(reflectionMapProgram, material0);
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "cameraPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));

	textureProgram = rt3d::initShaders("textured.vert", "textured.frag");
	skyboxProgram = rt3d::initShaders("cubeMap.vert", "cubeMap.frag");


	
	const char *cubeTexFiles[6] = {
		"Town-skybox/Town_bk.bmp", "Town-skybox/Town_ft.bmp", "Town-skybox/Town_rt.bmp", "Town-skybox/Town_lf.bmp", "Town-skybox/Town_up.bmp", "Town-skybox/Town_dn.bmp"
	};
	loadCubeMap(cubeTexFiles, &skybox[0]);

	
	vector<GLfloat> verts;
	vector<GLfloat> norms;
	vector<GLfloat> tex_coords;
	vector<GLuint> indices;
	rt3d::loadObj("cube.obj", verts, norms, tex_coords, indices);
	cubeIndexCount = indices.size();
	textures[0] = loadBitmap("fabric.bmp");
	meshObjects[0] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), tex_coords.data(), cubeIndexCount, indices.data());

	textures[2] = loadBitmap("studdedmetal.bmp");


	verts.clear(); norms.clear(); tex_coords.clear(); indices.clear();
	rt3d::loadObj("bunny-5000.obj", verts, norms, tex_coords, indices);
	bunnyIndexCount = indices.size();
	meshObjects[2] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), nullptr, bunnyIndexCount, indices.data());

	//create vertice buffer
	unsigned int vertbuffer;
	glGenBuffers(1, &vertbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertbuffer);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

	//set vertice buffer to attribute 0
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	//create normal buffer
	unsigned int normbuffer;
	glGenBuffers(1, &normbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normbuffer);
	glBufferData(GL_ARRAY_BUFFER, norms.size() * sizeof(float), norms.data(), GL_STATIC_DRAW);

	//set normal buffer to 2 (defined in RT3D header file)
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE , sizeof(float) * 2, 0);


	// Binding the vertex
	glBindBuffer(GL_ARRAY_BUFFER, vertbuffer);
	glVertexPointer(3, GL_FLOAT, sizeof(float) * 3, NULL); // Vertex start position address


	//create an interleaved vnc buffer
	unsigned int vncBuffer;
	glGenBuffers(1, &vncBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vncBuffer);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * 10 * sizeof(float), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * 3, verts.data());  // add vertice data
	glBufferSubData(GL_ARRAY_BUFFER, verts.size() * 3 * sizeof(float), verts.size() * 3 * sizeof(float), norms.data()); // then normal data
	glBufferSubData(GL_ARRAY_BUFFER, verts.size() * 3 * sizeof(float), verts.size() * 4 * sizeof(float), tex_coords.data()); // then color data




	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Binding tex handles to tex units to samplers under programmer control
	// set cubemap sampler to texture unit 1, arbitrarily
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "cubeMap");
	glUniform1i(uniformIndex, 1);
	// set tex sampler to texture unit 0, arbitrarily
	uniformIndex = glGetUniformLocation(reflectionMapProgram, "texMap");
	glUniform1i(uniformIndex, 0);
	// Now bind textures to texture units
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[2]);
	

}

glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::sin(r*DEG_TO_RADIAN), pos.y, pos.z - d * std::cos(r*DEG_TO_RADIAN));
}

glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::cos(r*DEG_TO_RADIAN), pos.y, pos.z + d * std::sin(r*DEG_TO_RADIAN));
}

void update(void) {
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	//camera controls
	if (keys[SDL_SCANCODE_W]) eye = moveForward(eye, r, 0.1f); // forward
	if (keys[SDL_SCANCODE_S]) eye = moveForward(eye, r, -0.1f); // backwards

	if (keys[SDL_SCANCODE_A]) eye = moveRight(eye, r, -0.1f); // left
	if (keys[SDL_SCANCODE_D]) eye = moveRight(eye, r, 0.1f); // right

	if (keys[SDL_SCANCODE_R]) eye.y += 0.1; // up
	if (keys[SDL_SCANCODE_F]) eye.y -= 0.1; // down
	
	//light controls
	if (keys[SDL_SCANCODE_UP]) lightPos[2] -= 0.1; // forward
	if (keys[SDL_SCANCODE_DOWN]) lightPos[2] += 0.1; // backwards

	if (keys[SDL_SCANCODE_LEFT]) lightPos[0] -= 0.1; // left
	if (keys[SDL_SCANCODE_RIGHT]) lightPos[0] += 0.1; // right

	if (keys[SDL_SCANCODE_Y]) lightPos[1] += 0.1; // up
	if (keys[SDL_SCANCODE_H]) lightPos[1] -= 0.1; // down

	//shader controls
	if (keys[SDL_SCANCODE_1]) shaderController = 1; //gouraud shader
	if (keys[SDL_SCANCODE_2]) shaderController = 2; // phong shader
	if (keys[SDL_SCANCODE_3]) shaderController = 3; // toon shader
	if (keys[SDL_SCANCODE_4]) shaderController = 4; // environment map shader
	if (keys[SDL_SCANCODE_5]) shaderController = 5; // refraction map shader

	if (keys[SDL_SCANCODE_COMMA]) r -= 1.0f;
	if (keys[SDL_SCANCODE_PERIOD]) r += 1.0f;


	// for wireframe mode
	if (keys[SDL_SCANCODE_9]) { // on
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
	}
	if (keys[SDL_SCANCODE_0]) { // off
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
	}

}

void drawGouraud(glm::vec4 tmp, glm::mat4 projection)
{
	// draw the Gouraud shaded bunny
	glUseProgram(gouraudShaderProgram);

	//set the light and projection matrices to the shader
	rt3d::setLightPos(gouraudShaderProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(gouraudShaderProgram, "projection", glm::value_ptr(projection));

	//set modelview
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(gouraudShaderProgram, "modelview", glm::value_ptr(mvStack.top()));

	//set material and draw the object
	rt3d::setMaterial(gouraudShaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	mvStack.pop();
}
void drawPhong(glm::vec4 tmp, glm::mat4 projection)
{
	// draw the Phong shaded bunny
	glUseProgram(phongShaderProgram);

	//set the light and projection matrices to the shader
	rt3d::setLightPos(phongShaderProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));

	//set modelview
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "modelview", glm::value_ptr(mvStack.top()));

	//set material and draw the object
	rt3d::setMaterial(phongShaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	mvStack.pop();
}
void drawToon(glm::vec4 tmp, glm::mat4 projection)
{

	// draw the toon shaded bunny
	glUseProgram(toonShaderProgram);

	//set the light and projection matrices to the shader
	rt3d::setLightPos(toonShaderProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(toonShaderProgram, "projection", glm::value_ptr(projection));

	//set modelview
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(toonShaderProgram, "modelview", glm::value_ptr(mvStack.top()));

	//set material and draw the object
	rt3d::setMaterial(toonShaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	mvStack.pop();

}
void drawMapped(glm::vec4 tmp, glm::mat4 projection)
{
	// draw a rotating cube to be environment/reflection mapped (now the bunny)
	glUseProgram(reflectionMapProgram);
	rt3d::setUniformMatrix4fv(reflectionMapProgram, "projection", glm::value_ptr(projection));

	//pass the light and pass it to the shader
	rt3d::setLightPos(reflectionMapProgram, glm::value_ptr(tmp));

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 20.0f, 20.0f));
	rt3d::setUniformMatrix4fv(reflectionMapProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(reflectionMapProgram, material1);

	//find the model matrix for the reflection shader
	glm::mat4 modelMatrix(1.0);
	mvStack.push(mvStack.top());
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, 1.0f, -3.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(20.0f, 20.0f, 20.0f));
	mvStack.top() = mvStack.top() * modelMatrix;
	rt3d::setUniformMatrix4fv(reflectionMapProgram, "modelMatrix", glm::value_ptr(mvStack.top()));

	//draw the object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);

	//two pops for both pushes
	mvStack.pop();
	mvStack.pop();
}
void drawRefracted(glm::vec4 tmp, glm::mat4 projection)
{
	// draw a rotating cube to be environment/reflection mapped (now the bunny)
	glUseProgram(refractionShaderProgram);
	rt3d::setUniformMatrix4fv(refractionShaderProgram, "projection", glm::value_ptr(projection));

	//pass the light and pass it to the shader
	rt3d::setLightPos(refractionShaderProgram, glm::value_ptr(tmp));

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 20.0f, 20.0f));
	rt3d::setUniformMatrix4fv(refractionShaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(refractionShaderProgram, material1);

	//find the model matrix for the reflection shader
	glm::mat4 modelMatrix(1.0);
	mvStack.push(mvStack.top());
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, 1.0f, -3.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(20.0f, 20.0f, 20.0f));
	mvStack.top() = mvStack.top() * modelMatrix;
	rt3d::setUniformMatrix4fv(refractionShaderProgram, "modelMatrix", glm::value_ptr(mvStack.top()));

	//draw the object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);

	//two pops for both pushes
	mvStack.pop();
	mvStack.pop();
}

void draw(SDL_Window * window) {
	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(60.0f*DEG_TO_RADIAN), 800.0f / 600.0f, 1.0f, 150.0f);

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	at = moveForward(eye, r, 1.0f);
	mvStack.top() = glm::lookAt(eye, at, up);

	// skybox as single cube using cube map
	glUseProgram(skyboxProgram);
	rt3d::setUniformMatrix4fv(skyboxProgram, "projection", glm::value_ptr(projection));

	glDepthMask(GL_FALSE); // make sure writing to update depth test is off
	glm::mat3 mvRotOnlyMat3 = glm::mat3(mvStack.top());
	mvStack.push(glm::mat4(mvRotOnlyMat3));

	glCullFace(GL_FRONT); // drawing inside of cube!
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox[0]);
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(1.5f, 1.5f, 1.5f));
	rt3d::setUniformMatrix4fv(skyboxProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();
	glCullFace(GL_BACK); // drawing inside of cube!


						 // back to remainder of rendering
	glDepthMask(GL_TRUE); // make sure depth test is on


	glUseProgram(shaderProgram);
	rt3d::setUniformMatrix4fv(shaderProgram, "projection", glm::value_ptr(projection));

	glm::vec4 tmp = mvStack.top()*lightPos;
	light0.position[0] = tmp.x;
	light0.position[1] = tmp.y;
	light0.position[2] = tmp.z;
	rt3d::setLightPos(shaderProgram, glm::value_ptr(tmp));

	// draw a small cube block at lightPos
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(lightPos[0], lightPos[1], lightPos[2]));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(0.25f, 0.25f, 0.25f));
	rt3d::setUniformMatrix4fv(shaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(shaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();

	// draw a cube for ground plane
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-10.0f, -0.1f, -10.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 0.1f, 20.0f));
	rt3d::setUniformMatrix4fv(shaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(shaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	//draw according to which shader is being utilised
	if (shaderController == 1) drawGouraud(tmp, projection);
	if (shaderController == 2) drawPhong(tmp, projection);
	if (shaderController == 3) drawToon(tmp, projection);
	if (shaderController == 4) drawMapped(tmp, projection);
	if (shaderController == 5) drawRefracted(tmp, projection);

	mvStack.pop(); // initial matrix
	glDepthMask(GL_TRUE);

	SDL_GL_SwapWindow(window); // swap buffers

}


// Program entry point
int main(int argc, char *argv[]) {
	SDL_Window * hWindow; // window handle
	SDL_GLContext glContext; // OpenGL context handle
	hWindow = setupRC(glContext); // Create window and render context 

								  // Required on Windows *only* init GLEW to access OpenGL beyond 1.1
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) { // glewInit failed, something is seriously wrong
		std::cout << "glewInit failed, aborting." << endl;
		exit(1);
	}
	cout << glGetString(GL_VERSION) << endl;

	init();

	bool running = true; // set running to true
	SDL_Event sdlEvent;  // variable to detect SDL events
	while (running) {	// the event loop
		while (SDL_PollEvent(&sdlEvent)) {
			if (sdlEvent.type == SDL_QUIT)
				running = false;
		}
		update();
		draw(hWindow); // call the draw function
	}

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(hWindow);
	SDL_Quit();
	return 0;
}