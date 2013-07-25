#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"

#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;
int a=0, b=0, c=0, d=0;
float rot = 0;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;
Terrain* _terrainJalan;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}

//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}
GLuint loadtextures(const char *filename, int width, int height) {//buat ngambil dari file image untuk jadi texture
GLuint texture;

unsigned char *data;
FILE *file;

file = fopen(filename, "rb");
if (file == NULL)
return 0;

data = (unsigned char *) malloc(width * height * 3);  //file pendukung texture
fread(data, width * height * 3, 1, file);

fclose(file);

glGenTextures(1, &texture);//generet (dibentuk)
glBindTexture(GL_TEXTURE_2D, texture);//binding (gabung)
glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
GL_LINEAR_MIPMAP_NEAREST);//untuk membaca gambar jadi text dan dapat dibaca dengan pixel
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB,
GL_UNSIGNED_BYTE, data);

data = NULL;

return texture;
}

void matahari(){

//matahari
 glPushMatrix();
 glColor3f(1,1,0);
 glutSolidSphere(2.0,100,10);
 glPopMatrix();
}

void markajalan(void) {

// marka jalan
glPushMatrix();
glScaled(1, 0.05,0.3);
glTranslatef(2.4,2.5,67);
glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
glColor3f(1,1,1);
glutSolidCube(5.0);
glPopMatrix();

}

void pohon(){
//batang-awan
glColor3f(0.8, 0.5, 0.2);
glPushMatrix();
glScalef(0.2, 2, 0.2);
glutSolidSphere(1.0, 20, 16);
glPopMatrix();
//batang-akhir

//daun
glColor3f(0.0, 1.0, 0.0);
glPushMatrix();
glScalef(1, 1, 1.0);
glTranslatef(0, 1, 0);
glRotatef(270, 1, 0, 0);
glutSolidCone(1,3,10,1);
glPopMatrix();

glPushMatrix();
glScalef(1, 1, 1.0);
glTranslatef(0, 2, 0);
glRotatef(270, 1, 0, 0);
glutSolidCone(1,3,10,1);
glPopMatrix();

glPushMatrix();
glScalef(1, 1, 1.0);
glTranslatef(0, 3, 0);
glRotatef(270, 1, 0, 0);
glutSolidCone(1,3,10,1);
glPopMatrix();
//daun-akhir
}

void rumah(){

//tembok
 glPushMatrix();
 glColor3f(0.7, 0.5, 0.3);
 glRotatef(50,0,1,0);
 glutSolidCube(3);

//pintu
glPushMatrix();
glColor3f(0.5,0.8,0);
glTranslatef(-0.70,-0.85,1.46);
glScalef(9,23,1);
glutSolidCube(0.1);
glPopMatrix();

//list-pintu
glPushMatrix();
glColor3f(0.0,0.0,0.0);
glTranslatef(-0.70,0.5,1.46);
glScalef(9,1,1);
glutSolidCube(0.1);
glPopMatrix();

//pegangan-pintu
glPushMatrix();
glColor3f(0.0,0.0,0.0);
glTranslatef(-1,-0.5,1.46);
glScalef(1,2,1);
glutSolidCube(0.1);
glPopMatrix();

//jendela
glPushMatrix();
glColor3f(0, 0, 0);
glTranslatef(0.7,0.4,1.46);
glScalef(9, 3.7,1);
glutSolidCube(0.1);
glPopMatrix();

glPushMatrix();
glColor3f(0,0,0);
glTranslatef(0.7,0,1.46);
glScalef(9, 3.7,1);
glutSolidCube(0.1);
glPopMatrix();

glPushMatrix();
glColor3f(0,0,0);
glTranslatef(0.5,0,1.46);
glScalef(3.7, 3.7,1);
glutSolidCube(0.1);
glPopMatrix();
glPopMatrix();

//atap
 glPushMatrix();
 glColor3f(0.2, 0.5, 0.5);
 glRotatef(5,0,1,0);
 glTranslatef(0,1.5,0);
 glScalef(3,1.3,3);
 glutSolidOctahedron();
 glPopMatrix();

}


void birds()
 {
    glPushMatrix();
    glTranslatef(19,11,-4);
    glRotatef(90,0,1,0);
    glScalef(1,1,1);
    glPopMatrix();
  }


void pager()
{
 //Pager depan kanan
              glPushMatrix();
              glTranslatef(11,-5.5,13);
              glColor3f(0.3,0.2,0);
              glScalef(28,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

              glPushMatrix();
              glTranslatef(11,-0.5,13);
              glColor3f(1,0,0);
              glScalef(28,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

 for (float dep=0.5;dep<28;dep+=2)
       {
              glPushMatrix();
              glTranslatef(dep-3,-3,13);
              glColor3f(0.7,0.3,0);
              glScalef(1,7,0.5);
              glutSolidCube(1);
              glPopMatrix();
             }

 //pager depan kiri
              glPushMatrix();
              glTranslatef(-15,-5.5,13);
              glColor3f(0.3,0.2,0);
              glScalef(13,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

              glPushMatrix();
              glTranslatef(-15,-0.5,13);
              glColor3f(1,0,0);
              glScalef(13,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

  for (float dep2=-4;dep2>-18;dep2-=2)
       {
              glPushMatrix();
              glTranslatef(dep2-5,-3,13);
              glColor3f(0.7,0.3,0);
              glScalef(1,7,0.5);
              glutSolidCube(1);
              glPopMatrix();
       }

  //pagar belakang
              glPushMatrix();
              glTranslatef(1.5,-5.5,-30);
              glColor3f(0.3,0.2,0);
              glScalef(45,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

              glPushMatrix();
              glTranslatef(1.5,-0.5,-30);
              glColor3f(1,0,0);
              glScalef(45,0.5,0.75);
              glutSolidCube(1);
              glPopMatrix();

  for (float dep=-10;dep<37;dep+=2)
       {
              glPushMatrix();
              glTranslatef(dep-11,-3,-30);
              glColor3f(0.7,0.3,0);
              glScalef(1,7,0.5);
              glutSolidCube(1);
              glPopMatrix();
       }

 //pagar samping kanan
              glPushMatrix();
              glTranslatef(25,-5.5,-7);
              glColor3f(0.3,0.2,0);
              glScalef(0.75,0.5,41);
              glutSolidCube(1);
              glPopMatrix();

              glPushMatrix();
              glTranslatef(25,-0.5,-7);
              glColor3f(1,0,0);
              glScalef(0.75,0.5,41);
              glutSolidCube(1);
              glPopMatrix();

  for (float samp=-13;samp<30;samp+=2)
       {
              glPushMatrix();
              glTranslatef(25,-3,samp+(-15));
              glColor3f(0.7,0.3,0);
              glScalef(0.5,7,1);
              glutSolidCube(1);
              glPopMatrix();
  }
//pagar samping kiri
              glPushMatrix();
              glTranslatef(-22,-5.5,-7);
              glColor3f(0.3,0.2,0);
              glScalef(0.75,0.5,41);
              glutSolidCube(1);
              glPopMatrix();

              glPushMatrix();
              glTranslatef(-22,-0.5,-7);
              glColor3f(1,0,0);
              glScalef(0.75,0.5,41);
              glutSolidCube(1);
              glPopMatrix();

  for (float samp=-13;samp<29;samp+=2)
       {
              glPushMatrix();
              glTranslatef(-22,-3,samp+(-15));
              glColor3f(0.7,0.3,0);
              glScalef(0.5,7,1);
              glutSolidCube(1);
              glPopMatrix();
                }

}

void pohon2()
{
//batang
GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH);

glPushMatrix();
glColor3f(0.7,0.3,0);
glRotatef(270,1,0,0);
gluCylinder(pObj, 1, 0.7, 10, 20, 15);
glPopMatrix();

//ranting

glPushMatrix();
glColor3ub(104,70,14);
glTranslatef(0,7,0);
glRotatef(330,1,0,0);
gluCylinder(pObj, 0.6, 0.1, 7, 25, 25);
glPopMatrix();

//daun
glPushMatrix();
glColor3f(0,1,0.3);
glScaled(4, 3, 5);
glTranslatef(0,4.7,0.4);
glutSolidDodecahedron();
glPopMatrix();
}


unsigned int LoadTextureFromBmpFile(char *filename);

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, -100.0, 0.0, 1.0, 0.0);


    glPushMatrix();

    //glBindTexture(GL_TEXTURE_2D, texture[1]); //untuk mmanggil texture
    //drawSceneTanah(_terrain, 0.804f, 0.53999999f, 0.296f);
    drawSceneTanah(_terrain, 0.4902f, 0.4683f,0.4594f);
    glPopMatrix();

    glPushMatrix();

    drawSceneTanah(_terrainJalan, 0.3f, 0.53999999f, 0.0654f);
    glPopMatrix();

    glPushMatrix();
    drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
    glPopMatrix();

    glPushMatrix();

    //glBindTexture(GL_TEXTURE_2D, texture[0]);
    drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
    glPopMatrix();

	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

    //burung
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-12,50,-30);
    glTranslatef(-6,30,0);
    birds();
    glPopMatrix();

    //matahari
    glPushMatrix();
    glTranslatef(-10,10,-20);
    glTranslatef(-7,50,0);
	matahari();
	glPopMatrix();

    //pager
    glPushMatrix();
    glTranslatef(20,10,15);
    pager();
	glPopMatrix();

    //rumah
    {
    glPushMatrix();
    glScalef(7,7,7);
    glTranslatef(2, 2, 0);
    glRotatef(-50, 0, 1, 0);
    rumah();
    glPopMatrix();
    }

    //markajalan
    glPushMatrix();
    glTranslatef(250,1,8);
    glScalef(3, 2, 2);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(220,1,8);
    glScalef(3, 2, 2);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(190,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(160,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(130,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(100,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(60,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan rotasi-1
    glPushMatrix();
    glTranslatef(147,1,90);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi-2
    glPushMatrix();
    glTranslatef(147,1,120);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi-3
    glPushMatrix();
    glTranslatef(147,1,150);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi1
    glPushMatrix();
    glTranslatef(147,1,30);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi2
    glPushMatrix();
    glTranslatef(147,1,0);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi3
    glPushMatrix();
    glTranslatef(147,1,-30);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi4
    glPushMatrix();
    glTranslatef(147,1,-60);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi5
    glPushMatrix();
    glTranslatef(147,1,-90);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi6
    glPushMatrix();
    glTranslatef(147,1,-120);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi7
    glPushMatrix();
    glTranslatef(147,1,-150);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();

    //markajalan rotasi8
    glPushMatrix();
    glTranslatef(147,1,-180);
    glScalef(3, 3, 3);
    glRotatef(-90,0,1,0);
    markajalan();
    glPopMatrix();


    //markajalan
    glPushMatrix();
    glTranslatef(0,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(30,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-30,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-60,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-90,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-120,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-150,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-180,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //markajalan
    glPushMatrix();
    glTranslatef(-210,1,8);
    glScalef(3, 3, 3);
    markajalan();
    glPopMatrix();

    //halaman-kiri
    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(58, 0, -10);
    glScalef(1.5,1.5,1.5);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(54, 10, -70);
    glScalef(9, 9, 9);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(50, 10, -160);
    glScalef(9,9,9);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(10, 5, -160);
    glScalef(3,3,3);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-60, 10, -160);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-130, 10, -160);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180, 5, -160);
    glScalef(3,3,3);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180, 10, -80);
    glScalef(9, 9, 9);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180 , 0, -25);
    glScalef(3,3,3);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180, 10, 10);
    glScalef(9, 9, 9);
    pohon();
    glPopMatrix();
    //halaman-kiriberes

    //halaman-kirisebrang
    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180 , 0, 120);
    glScalef(3,3,3);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-180, 10, 160);
    glScalef(9, 9, 9);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-120 , 0, 175);
    glScalef(2.5,2.5,2.5);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(-50, 10, 175);
    glScalef(9, 9, 9);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(10 , 0, 175);
    glScalef(2.5,2.5,2.5);
    pohon2();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(60, 10, 175);
    glScalef(5,5,5);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(60, 10, 140);
    glScalef(5,5,5);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(60, 7, 115);
    glScalef(4,4,4);
    pohon();
    glPopMatrix();

    //halaman-kanan
    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(120, 10, -160);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(170, 10, -160);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, -160);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, -110);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, -60);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, -10);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, 30);
    glScalef(8, 8, 8);
    pohon();
    glPopMatrix();

    //halaman-kanansebrang
    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, 90);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, 130);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(230, 10, 170);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(190, 10, 175);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(150, 10, 175);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef(110, 10, 175);
    glScalef(10, 10, 10);
    pohon();
    glPopMatrix();

    glEnd();
    glutSwapBuffers(); /*Fungsi berikutnya adalah glutSwapBuffers(),
    yaitu digunakan untuk menukar bagian belakan buffer menjadi buffer
    layar (screen buffer). Dalam modus double-buffered, perintah
    menggambar pada layar, tetapi menggambar pada buffer belakang layar.
    Pada saat ingin menampilkan gambar, gambar yang berada di buffer
    belakang layar dipindahkan ke buffer layar, sehingga menghasilkan
    animasi yang sempurna.*/
}



void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);
	_terrainJalan = loadTerrain("heightmap2.bmp", 20);

	//binding texture


}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Tugas Besar Grafkom-Rumah 3D");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

//    glutTimerFunc(25, putar, 0);
	glutMainLoop();
	return 0;
}
