// fragment shading of sphere model
//
// Modified by David M. Chelberg for style
// Extensively modified to add multiple light sources, and multiple
// objects, including an emissive one (glows) to show position of
// moving nearby light source.
//
// last-modified: Wed Nov 13 14:35:39 2013
//

#include "Angel.h"

const int NumTimesToSubdivide = 5;
const int NumTriangles        = 4096;  // (4 faces)^(NumTimesToSubdivide + 1)
const int NumVertices         = 3 * NumTriangles;

/*
// What effect will the following have?
const int NumTimesToSubdivide = 2;
const int NumTriangles        = 64;  // (4 faces)^(NumTimesToSubdivide + 1)
const int NumVertices         = 3 * NumTriangles;
*/

typedef Angel::vec4 point4;
typedef Angel::vec4 color4;

point4 points[NumVertices];
vec4   normals[NumVertices];

// Viewing transformation parameters
GLfloat radius = 8.0;  // initial distane of camera from origin
GLfloat theta = 0.0;   // initial orientation of camera
GLfloat phi = 0.0;     // initial orientation of camera
GLfloat  fovy = 45.0;  // Field-of-view in Y direction angle (in degrees)
GLfloat aspect;        // aspect ratio of window

const GLfloat  dr = 5.0 * DegreesToRadians;  // how much to rotate
					     // camera on each keypress

// Camera Model-view matrix uniform location
GLuint Camera;
// Object Model-view matrix uniform location
GLuint ModelView;
// projection matrix uniform location
GLuint Projection;

// Position of light1
point4 light_position(0.0, 0.0, 10.0, 1.0);
// If you want a non-positional light use 0.0 for fourth value
//point4 light_position(0.0, 0.0, 1.0, 0.0);

// Initialize first light lighting parameters
color4 light_ambient(0.2, 0.2, 0.2, 1.0);
color4 light_diffuse(0.5, 0.5, 0.5, 1.0);
color4 light_specular(0.6, 0.6, 0.6, 1.0);

// Position of light2 (it will change, so needs to be global).
point4 light2_position(3.0, 0.0, 0.0, 1.0);
GLuint Light2_Pos; // uniform location

// Initialize second light lighting parameters
color4 light2_ambient(0.2, 0.0, 0.2, 1.0);
color4 light2_diffuse(0.7, 0.3, 0.7, 1.0);
color4 light2_specular(0.6, 0.0, 0.6, 1.0);

// Material Properties for spheres
color4 material_ambient(1.0, 0.0, 1.0, 1.0);
color4 material_diffuse(1.0, 0.8, 0.0, 1.0);
color4 material_specular(1.0, 0.0, 1.0, 1.0);
float  material_shininess = 500.0;

// Next variables used to move light2.
GLfloat light_theta=0.0;  // the angle of light2
GLfloat light_incr= 0.001;// the amount to move light2 in radians/millisecond
bool rotate=false;        // are we animating light2?

GLuint Light1;     // uniform location to turn light on or off
GLuint Light2;     // uniform location to turn light on or off

bool light1=true;  // Is light1 on?
bool light2=true;  // Is light2 on?

// Next two variables are to set whether an object has an emissive
// term in the light equation (does it glow?, and what color?).
// Default emissive color:
color4 emissive(0.7, 0.0, 0.7, 1.0);
// Default emissive color if not emissive object:
color4 emissive_off(0.0, 0.0, 0.0, 1.0);

GLuint Material_Emiss; // uniform location of emissive property of
		       // surface

//----------------------------------------------------------------------------
int Index = 0;

void triangle(const point4& a, const point4& b, const point4& c)
{
  normals[Index] = a;  normals[Index].w=0.0;  points[Index] = a;  Index++;
  normals[Index] = b;  normals[Index].w=0.0;  points[Index] = b;  Index++;
  normals[Index] = c;  normals[Index].w=0.0;  points[Index] = c;  Index++;
}

//----------------------------------------------------------------------------

point4 unit(const point4& p)
{
  float len = p.x*p.x + p.y*p.y + p.z*p.z;
    
  point4 t;
  if (len > DivideByZeroTolerance) {
    t = p / sqrt(len);
    t.w = 1.0;
  }

  return t;
}

void divide_triangle(const point4& a, const point4& b,
		     const point4& c, int count)
{
  if (count > 0) {
    point4 v1 = unit(a + b);
    point4 v2 = unit(a + c);
    point4 v3 = unit(b + c);
    divide_triangle( a, v1, v2, count - 1);
    divide_triangle( c, v2, v3, count - 1);
    divide_triangle( b, v3, v1, count - 1);
    divide_triangle(v1, v3, v2, count - 1);
  }
  else {
    triangle(a, b, c);
  }
}

void tetrahedron(int count)
{
  point4 v[4] = {
    vec4(0.0, 0.0, 1.0, 1.0),
    vec4(0.0, 0.942809, -0.333333, 1.0),
    vec4(-0.816497, -0.471405, -0.333333, 1.0),
    vec4(0.816497, -0.471405, -0.333333, 1.0)
  };

  divide_triangle(v[0], v[1], v[2], count);
  divide_triangle(v[3], v[2], v[1], count);
  divide_triangle(v[0], v[3], v[1], count);
  divide_triangle(v[0], v[2], v[3], count);
}

//----------------------------------------------------------------------------
// OpenGL initialization
void init()
{
  // Subdivide a tetrahedron into a sphere
  tetrahedron(NumTimesToSubdivide);

  // Create a vertex array object
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Create and initialize a buffer object
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(points) + sizeof(normals),
	       NULL, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(points), points);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(points),
		  sizeof(normals), normals);

  // Load shaders and use the resulting shader program
  GLuint program = InitShader("vshader.glsl", "fshader.glsl");
  glUseProgram(program);
	
  // set up vertex arrays
  GLuint vPosition = glGetAttribLocation(program, "vPosition");
  glEnableVertexAttribArray(vPosition);
  glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, 0,
			BUFFER_OFFSET(0));

  GLuint vNormal = glGetAttribLocation(program, "vNormal"); 
  glEnableVertexAttribArray(vNormal);
  glVertexAttribPointer(vNormal, 4, GL_FLOAT, GL_FALSE, 0,
			BUFFER_OFFSET(sizeof(points)));

  color4 ambient_product = light_ambient * material_ambient;
  color4 diffuse_product = light_diffuse * material_diffuse;
  color4 specular_product = light_specular * material_specular;

  color4 ambient2_product = light2_ambient * material_ambient;
  color4 diffuse2_product = light2_diffuse * material_diffuse;
  color4 specular2_product = light2_specular * material_specular;

  glUniform4fv(glGetUniformLocation(program, "AmbientProduct"),
	       1, ambient_product);
  glUniform4fv(glGetUniformLocation(program, "DiffuseProduct"),
	       1, diffuse_product);
  glUniform4fv(glGetUniformLocation(program, "SpecularProduct"),
	       1, specular_product);
	
  glUniform4fv(glGetUniformLocation(program, "LightPosition"),
	       1, light_position);

  glUniform4fv(glGetUniformLocation(program, "AmbientProduct2"),
	       1, ambient2_product);
  glUniform4fv(glGetUniformLocation(program, "DiffuseProduct2"),
	       1, diffuse2_product);
  glUniform4fv(glGetUniformLocation(program, "SpecularProduct2"),
	       1, specular2_product);
	
  Light2_Pos = glGetUniformLocation(program, "LightPosition2");
  glUniform4fv(Light2_Pos, 1, light2_position);

  glUniform1f(glGetUniformLocation(program, "Shininess"),
	      material_shininess);
		 
  // If surface glows, it should have an emissive term in the material property.
  Material_Emiss = glGetUniformLocation(program, "emissive");
  glUniform4fv(Material_Emiss, 1, emissive_off);

  Light1 = glGetUniformLocation(program, "light1");
  glUniform1i(Light1,light1);

  Light2 = glGetUniformLocation(program, "light2");
  glUniform1i(Light2,light2);

  // Retrieve transformation uniform variable locations
  Camera = glGetUniformLocation(program, "Camera");
  ModelView = glGetUniformLocation(program, "ModelView");
  Projection = glGetUniformLocation(program, "Projection");
    
  glEnable(GL_DEPTH_TEST);
    
  glClearColor(1.0, 1.0, 1.0, 1.0); // white background
}

//----------------------------------------------------------------------------
extern "C" void idle()
{
  static GLint time=glutGet(GLUT_ELAPSED_TIME);
  if (rotate) {
    light_theta += light_incr*(glutGet(GLUT_ELAPSED_TIME)-time);

    // Light2 moves in an ellipse in the xy plane major axis = 3
    // units, minor axis = 2 units.
    light2_position=point4(3.0*cos(light_theta), 2.0*sin(light_theta), 0.0, 1.0);

    glUniform4fv(Light2_Pos, 1, light2_position);
  }
  time = glutGet(GLUT_ELAPSED_TIME);

  glutPostRedisplay();
}

//----------------------------------------------------------------------------
extern "C" void display()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mat4 projection = Perspective(fovy, aspect, 0.1, 100.0);
  glUniformMatrix4fv(Projection, 1, GL_TRUE, projection);


  point4  eye(radius*sin(theta)*cos(phi),
	      radius*sin(theta)*sin(phi),
	      radius*cos(theta),
	      1.0);
  point4  at(0.0, 0.0, 0.0, 1.0);
  vec4    up(0.0, 1.0, 0.0, 0.0);

  mat4 camera = LookAt(eye, at, up);
  glUniformMatrix4fv(Camera, 1, GL_TRUE, camera);

  // First specify the location, orientation, and scale of the object
  mat4 model_view = Translate(0.0, 0.0, 0.0);
  glUniformMatrix4fv(ModelView, 1, GL_TRUE, model_view);
  glDrawArrays(GL_TRIANGLES, 0, NumVertices);

  // Reuse model_view matrix.
  // First specify the location, orientation, and scale of the object
  model_view = Translate(2.0, 0.0, 0.0);
  glUniformMatrix4fv(ModelView, 1, GL_TRUE, model_view);
  glDrawArrays(GL_TRIANGLES, 0, NumVertices);

  // Both light sources are emissive.
  glUniform4fv(Material_Emiss, 1, light_diffuse);
  // Sphere for First Light source (fixed)
  // Move third sphere to where light1 is positioned, and make it an
  // emissive object.
  // Only draw if it is a near (positional) light source as opposed to
  // a distant light source.
  if (light_position.w == 1.0) {
    // This code will display the light source as a sphere at the
    // location of the light with emissive properties of its light. 
    model_view = Translate(light_position)*Scale(0.1, 0.1, 0.1);
    glUniformMatrix4fv(ModelView, 1, GL_TRUE, model_view);
    glDrawArrays(GL_TRIANGLES, 0, NumVertices);
  }

  // Move fourth sphere to where light2 is positioned, and make it an
  // emissive object.
  glUniform4fv(Material_Emiss, 1, light2_diffuse);
  // This code will display the light source as a sphere at the
  // location of the light with emissive properties of its light.
  // Note: light_2 is animated to move in an ellipse.
  model_view = Translate(3.0*cos(light_theta), 2.0*sin(light_theta), 0.0)*Scale(0.1, 0.1, 0.1);
  glUniformMatrix4fv(ModelView, 1, GL_TRUE, model_view);
  glDrawArrays(GL_TRIANGLES, 0, NumVertices);

  // Turn emissive property off 
  glUniform4fv(Material_Emiss, 1, emissive_off);

  glutSwapBuffers();
}

//----------------------------------------------------------------------------
extern "C" void keyboard(unsigned char key, int x, int y)
{
  switch(key) {
  case 033: // Escape Key
  case '1': // Toggle light1
    light1=!light1;
    glUniform1i(Light1,light1);
    break;
  case '2': // Toggle light2
    light2=!light2;
    glUniform1i(Light2,light2);
    break;
  case 'q': case 'Q':
    exit(EXIT_SUCCESS);
    break;
    // increase/decrease distance of camera from origin
  case 'r': radius *= 1.5; break;
  case 'R': radius /= 1.5; break;
    // Next commands are to rotate camera about origin.
  case 'o': theta += dr; break;
  case 'O': theta -= dr; break;
  case 'p': phi += dr; break;
  case 'P': phi -= dr; break;
    // Next commands are to change "zoom" of camera
  case 'v': 
    fovy-=5; 
    if (fovy < 0) {
      // Min angle of view 1 degree
      fovy = 1;
    }
    break;
  case 'V': fovy+=5; break;
    if (fovy > 179) {
      // Max angle of view 179 degrees
      fovy = 179;
    }
    break;

    // toggle animation (movement) of light2
  case 'a':
    rotate=!rotate;
    break;

  case ' ':  // reset values to their defaults
    fovy = 45.0;
    radius = 8.0;

    light_theta=0.0;
    light2_position=point4(3.0*cos(light_theta), 2.0*sin(light_theta), 0.0, 1.0);
    glUniform4fv(Light2_Pos, 1, light2_position);

    theta  = 0.0;
    phi    = 0.0;
    rotate = false;
    break;
  }
  glutPostRedisplay();
}

//----------------------------------------------------------------------------
extern "C" void reshape(int width, int height)
{
  glViewport(0, 0, width, height);

  aspect = GLfloat(width)/height;

  mat4 projection = Perspective(fovy, aspect, 0.1, 100.0);
  glUniformMatrix4fv(Projection, 1, GL_TRUE, projection);
}

//----------------------------------------------------------------------------
int main(int argc, char **argv)
{

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
  glutInitWindowSize(512, 512);
  glutCreateWindow("Sphere");

  glewInit();

  init();

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutIdleFunc(idle);
  glutMainLoop();
  return(EXIT_SUCCESS);
}
