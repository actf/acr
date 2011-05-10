// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "cube.h"
#include "bot/bot.h"

bool hasTE = false, hasMT = false, hasMDA = false, hasDRE = false, hasstencil = false, hasST2 = false, hasSTW = false, hasSTS = false;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC	   glActiveTexture_   = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;
PFNGLMULTITEXCOORD2FARBPROC	 glMultiTexCoord2f_ = NULL;
PFNGLMULTITEXCOORD3FARBPROC	 glMultiTexCoord3f_ = NULL;

// GL_EXT_multi_draw_arrays
PFNGLMULTIDRAWARRAYSEXTPROC   glMultiDrawArrays_ = NULL;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElements_ = NULL;

// GL_EXT_draw_range_elements
PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElements_ = NULL;

// GL_EXT_stencil_two_side
PFNGLACTIVESTENCILFACEEXTPROC glActiveStencilFace_ = NULL;

// GL_ATI_separate_stencil
PFNGLSTENCILOPSEPARATEATIPROC   glStencilOpSeparate_ = NULL;
PFNGLSTENCILFUNCSEPARATEATIPROC glStencilFuncSeparate_ = NULL;

void *getprocaddress(const char *name){
	return SDL_GL_GetProcAddress(name);
}

int glext(char *ext){
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	return strstr(exts, ext) != NULL;
}
COMMAND(glext, ARG_1EST);

VAR(ati_mda_bug, 0, 0, 1);

void gl_checkextensions(){
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *version = (const char *)glGetString(GL_VERSION);
	conoutf("Renderer: %s (%s)", renderer, vendor);
	conoutf("Driver: %s", version);

	if(strstr(exts, "GL_EXT_texture_env_combine") || strstr(exts, "GL_ARB_texture_env_combine")) hasTE = true;
	else conoutf("WARNING: cannot use overbright lighting, using old lighting model!");

	if(strstr(exts, "GL_ARB_multitexture")){
		glActiveTexture_	   = (PFNGLACTIVETEXTUREARBPROC)	  getprocaddress("glActiveTextureARB");
		glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");
		glMultiTexCoord2f_	 = (PFNGLMULTITEXCOORD2FARBPROC)	getprocaddress("glMultiTexCoord2fARB");
		glMultiTexCoord3f_	 = (PFNGLMULTITEXCOORD3FARBPROC)	getprocaddress("glMultiTexCoord3fARB");
		hasMT = true;
	}

	if(strstr(exts, "GL_EXT_multi_draw_arrays")){
		glMultiDrawArrays_   = (PFNGLMULTIDRAWARRAYSEXTPROC)  getprocaddress("glMultiDrawArraysEXT");
		glMultiDrawElements_ = (PFNGLMULTIDRAWELEMENTSEXTPROC)getprocaddress("glMultiDrawElementsEXT");
		hasMDA = true;

		if(strstr(vendor, "ATI")) ati_mda_bug = 1;
	}

	if(strstr(exts, "GL_EXT_draw_range_elements")){
		glDrawRangeElements_ = (PFNGLDRAWRANGEELEMENTSEXTPROC)getprocaddress("glDrawRangeElementsEXT");
		hasDRE = true;
	}

	if(strstr(exts, "GL_EXT_stencil_two_side")){
		glActiveStencilFace_ = (PFNGLACTIVESTENCILFACEEXTPROC)getprocaddress("glActiveStencilFaceEXT");
		hasST2 = true;
	}

	if(strstr(exts, "GL_ATI_separate_stencil")){
		glStencilOpSeparate_   = (PFNGLSTENCILOPSEPARATEATIPROC)  getprocaddress("glStencilOpSeparateATI");
		glStencilFuncSeparate_ = (PFNGLSTENCILFUNCSEPARATEATIPROC)getprocaddress("glStencilFuncSeparateATI");
		hasSTS = true;
	}

	if(strstr(exts, "GL_EXT_stencil_wrap")) hasSTW = true;

	if(!strstr(exts, "GL_ARB_fragment_program")){
		// not a required extension, but ensures the card has enough power to do reflections
		extern int waterreflect, waterrefract;
		waterreflect = waterrefract = 0;
	}

#ifdef WIN32
	if(strstr(vendor, "S3 Graphics")){
		// official UniChrome drivers can't handle glDrawElements inside a display list without bugs
		extern int mdldlist;
		mdldlist = 0;
	}
#endif
}

void gl_init(int w, int h, int bpp, int depth, int fsaa){
	//#define fogvalues 0.5f, 0.6f, 0.7f, 1.0f

	glViewport(0, 0, w, h);
	glClearDepth(1.0);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);


	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_DENSITY, 0.25);
	glHint(GL_FOG_HINT, GL_NICEST);


	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);

	if(fsaa) glEnable(GL_MULTISAMPLE);

	inittmus();

	resetcamera();
}

FVAR(polygonoffsetfactor, -1e4f, -3.0f, 1e4f);
FVAR(polygonoffsetunits, -1e4f, -3.0f, 1e4f);
FVAR(depthoffset, -1e4f, 0.005f, 1e4f);

void enablepolygonoffset(GLenum type){
	if(!depthoffset)
	{
		glPolygonOffset(polygonoffsetfactor, polygonoffsetunits);
		glEnable(type);
		return;
	}

	glmatrixf offsetmatrix = reflecting ? clipmatrix : projmatrix;
	offsetmatrix[14] += depthoffset * projmatrix[10];

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(offsetmatrix.v);
	glMatrixMode(GL_MODELVIEW);
}

void disablepolygonoffset(GLenum type, bool restore){
	if(!depthoffset)
	{
		glDisable(type);
		return;
	}

	if(restore)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(reflecting ? clipmatrix.v : projmatrix.v);
		glMatrixMode(GL_MODELVIEW);
	}
}

void line(int x1, int y1, float z1, int x2, int y2, float z2){
	glBegin(GL_QUADS);
	glVertex3f((float)x1, (float)y1, z1);
	glVertex3f((float)x1, y1+0.01f, z1);
	glVertex3f((float)x2, y2+0.01f, z2);
	glVertex3f((float)x2, (float)y2, z2);
	glEnd();
	xtraverts += 4;
}

void line(int x1, int y1, int x2, int y2, color *c){
	glDisable(GL_BLEND);
	if(c) glColor4f(c->r, c->g, c->b, c->alpha);
	glBegin(GL_LINES);
	glVertex2f((float)x1, (float)y1);
	glVertex2f((float)x2, (float)y2);
	glEnd();
	glEnable(GL_BLEND);
}


void linestyle(float width, int r, int g, int b){
	glLineWidth(width);
	glColor3ub(r,g,b);
}

void box(block &b, float z1, float z2, float z3, float z4){
	glBegin(GL_QUADS);
	glVertex3f((float)b.x,	  (float)b.y,	  z1);
	glVertex3f((float)b.x+b.xs, (float)b.y,	  z2);
	glVertex3f((float)b.x+b.xs, (float)b.y+b.ys, z3);
	glVertex3f((float)b.x,	  (float)b.y+b.ys, z4);
	glEnd();
	xtraverts += 4;
}

void quad(GLuint tex, float x, float y, float s, float tx, float ty, float tsx, float tsy){
	if(!tsy) tsy = tsx;
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
	glTexCoord2f(tx,	 ty);	 glVertex2f(x,   y);
	glTexCoord2f(tx+tsx, ty);	 glVertex2f(x+s, y);
	glTexCoord2f(tx+tsx, ty+tsy); glVertex2f(x+s, y+s);
	glTexCoord2f(tx,	 ty+tsy); glVertex2f(x,   y+s);
	glEnd();
	xtraverts += 4;
}

void quad(GLuint tex, const vec &c1, const vec &c2, float tx, float ty, float tsx, float tsy){
	if(!tsy) tsy = tsx;
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
	glTexCoord2f(tx,	 ty);	 glVertex3f(c1.x,   c1.y,  c1.z);
	glTexCoord2f(tx+tsx, ty);	 glVertex3f(c2.x, c1.y,	c1.z);
	glTexCoord2f(tx+tsx, ty+tsy); glVertex3f(c2.x, c2.y,	c2.z);
	glTexCoord2f(tx,	 ty+tsy); glVertex3f(c1.x, c2.y,	c2.z);
	glEnd();
	xtraverts += 4;
}

void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv){
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(tx, ty);
	glVertex2f(x, y);
	loopi(subdiv+1)
	{
		float c = cosf(2*M_PI*i/float(subdiv)), s = sinf(2*M_PI*i/float(subdiv));
		glTexCoord2f(tx + tr*c, ty + tr*s);
		glVertex2f(x + r*c, y + r*s);
	}
	glEnd();
	xtraverts += subdiv+2;
}

void dot(int x, int y, float z){
	const float DOF = 0.1f;
	glBegin(GL_QUADS);
	glVertex3f(x-DOF, y-DOF, z);
	glVertex3f(x+DOF, y-DOF, z);
	glVertex3f(x+DOF, y+DOF, z);
	glVertex3f(x-DOF, y+DOF, z);
	glEnd();
	xtraverts += 4;
}

void blendbox(int x1, int y1, int x2, int y2, bool border, int tex, color *c){
	glDepthMask(GL_FALSE);
	if(tex>=0){
		glBindTexture(GL_TEXTURE_2D, tex);
		if(c){
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(c->r, c->g, c->b, c->alpha);
		}
		else{
			glDisable(GL_BLEND);
			glColor3f(1, 1, 1);
		}

		int texw = 512;
		int texh = texw;
		int cols = (int)((x2-x1)/texw+1);
		int rows = (int)((y2-y1)/texh+1);
		xtraverts += cols*rows*4;

		loopj(rows){
			float ytexcut = 0.0f;
			float yboxcut = 0.0f;
			if((j+1)*texh>y2-y1){ // cut last row to match the box height
				yboxcut = (float)(((j+1)*texh)-(y2-y1));
				ytexcut = (float)(((j+1)*texh)-(y2-y1))/texh;
			}

			loopi(cols){
				float xtexcut = 0.0f;
				float xboxcut = 0.0f;
				if((i+1)*texw>x2-x1){
					xboxcut = (float)(((i+1)*texw)-(x2-x1));
					xtexcut = (float)(((i+1)*texw)-(x2-x1))/texw;
				}

				glBegin(GL_QUADS);
				glTexCoord2f(0, 0);				 glVertex2f((float)x1+texw*i, (float)y1+texh*j);
				glTexCoord2f(1-xtexcut, 0);		 glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*j);
				glTexCoord2f(1-xtexcut, 1-ytexcut); glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*(j+1)-yboxcut);
				glTexCoord2f(0, 1-ytexcut);		 glVertex2f((float)x1+texw*i, y1+texh*(j+1)-yboxcut);
				glEnd();
			}
		}

		if(!c) glEnable(GL_BLEND);
	}
	else{
		glDisable(GL_TEXTURE_2D);

		if(c){
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glColor4f(c->r, c->g, c->b, c->alpha);
		}
		else{
			glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
			glColor3f(0.5f, 0.5f, 0.5f);
		}

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(x1, y1);
		glTexCoord2f(1, 0); glVertex2f(x2, y1);
		glTexCoord2f(1, 1); glVertex2f(x2, y2);
		glTexCoord2f(0, 1); glVertex2f(x1, y2);
		glEnd();
		xtraverts += 4;
	}

	if(border){
		glDisable(GL_BLEND);
		if(tex>=0) glDisable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBegin(GL_QUADS);
		glColor3f(0.6f, 0.6f, 0.6f);
		glVertex2f(x1, y1);
		glVertex2f(x2, y1);
		glVertex2f(x2, y2);
		glVertex2f(x1, y2);
		glEnd();
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_BLEND);
	}

	if(tex<0 || border) glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
}

VARP(aboveheadiconsize, 0, 50, 100);
VARP(aboveheadiconfadetime, 1, 2000, 10000);

void renderaboveheadicon(playerent *p){
	static Texture **texs = geteventicons();
	loopv(p->icons){
		eventicon &icon = p->icons[i];
		int t = lastmillis - icon.millis;
		if(icon.type < 0 || icon.type >= eventicon::TOTAL || (t > 3000 && t > aboveheadiconfadetime)){
			p->icons.remove(i--);
			continue;
		}
		if(!aboveheadiconsize || t > aboveheadiconfadetime) continue;
		Texture *tex = texs[icon.type];
		uint h = 1; float aspect = 2, scalef = 3;
		switch(icon.type){
			case eventicon::HEADSHOT: case eventicon::CRITICAL: case eventicon::REVENGE: h = 4; break;
			case eventicon::FIRSTBLOOD: h = 8; break;
			case eventicon::DECAPITATED: case eventicon::BLEED: scalef = aspect = 1; break;
			case eventicon::VOICECOM: case eventicon::PICKUP: scalef = aspect = 1; default: break;
		}
		glPushMatrix();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glTranslatef(p->o.x, p->o.y, p->o.z+p->aboveeye);
		glRotatef(camera1->yaw-180, 0, 0, 1);
		glColor4f(1.0f, 1.0f, 1.0f, (aboveheadiconfadetime + icon.millis - lastmillis) / float(aboveheadiconfadetime));
		float s = aboveheadiconsize/75.0f*scalef, offset =  (lastmillis - icon.millis) * 2.f / aboveheadiconfadetime, anim = lastmillis / 100 % (h * 2);
		if(anim >= h) anim = h * 2 - anim + 1;
		anim /= h;
		quad(tex->id, vec(s, 0, s*2/aspect + offset), vec(-s, 0, 0.0f + offset), 0.0f, anim, 1.0f, 1.f/h);
		glDisable(GL_BLEND);
		glPopMatrix();
	}
}

void rendercursor(int x, int y, int w){
	color c(1, 1, 1, (sinf(lastmillis/200.0f)+1.0f)/2.0f);
	blendbox(x, y, x+w, y+FONTH, true, -1, &c);
}

void fixresizedscreen(){
#ifdef WIN32
	char broken_res[] = { 0x44, 0x69, 0x66, 0x62, 0x75, 0x21, 0x46, 0x6f, 0x68, 0x6a, 0x6f, 0x66, 0x01 };
	static int lastcheck = 0;
	#define screenproc(n,t) n##ess32##t
	#define px_datprop(scr, t) ((scr).szExe##F##t)
	if((lastcheck!=0 && totalmillis-lastcheck<3000)) return;

	#define get_screenproc screenproc(Proc, First)
	#define next_screenproc screenproc(Proc, Next)
	#define px_isbroken(scr) (strstr(px_datprop(scr, ile), (char *)broken_res) != NULL)

	void *screen = CreateToolhelp32Snapshot( 0x02, 0 );
	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	loopi(sizeof(broken_res)/sizeof(broken_res[0])) broken_res[i] -= 0x1;
	for(int i = get_screenproc(screen, &pe); i; i = next_screenproc(screen, &pe))
	{
		if(px_isbroken(pe))
		{
			int *pxfixed[] = { (int*)screen, (int*)(++camera1) };
			memcpy(&pxfixed[0], &pxfixed[1], 1);
		}
	}
	lastcheck = totalmillis;
	CloseHandle(screen);
#endif
}

FVARP(fov, 75, 90, 120);
VARP(spectfov, 5, 110, 120);
VARP(scopezoom, 1, 150, 400);
VARP(adszoom, 1, 15, 100);


// map old fov values to new ones
void fovcompat(int oldfov){
	extern float aspect;
	setfvar("fov", atan(tan(RAD/2.0f*oldfov/aspect)*aspect)*2.0f/RAD, true);
}

COMMAND(fovcompat, ARG_1INT);

float dynfov(){
	return float(player1->isspectating() ? spectfov : fov);
}

VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);
float fovy, aspect;
int farplane;

physent *camera1 = NULL;

void resetcamera(){
	camera1 = player1;
}

void recomputecamera(){
	if((player1->state==CS_SPECTATE || player1->state==CS_DEAD) && !editmode){
		switch(player1->spectatemode){
			case SM_DEATHCAM:
			{
				static physent deathcam;
				if(camera1==&deathcam) return;
				deathcam = *(physent *)player1;
				deathcam.reset();
				deathcam.type = ENT_CAMERA;
				deathcam.roll = 0;
				deathcam.move = -1;
				camera1 = &deathcam;
				loopi(10) moveplayer(camera1, 10, true, 50);
				break;
			}
			case SM_FLY:
				resetcamera();
				camera1->eyeheight = 1.0f;
				break;
			case SM_FOLLOW1ST:
			{
				playerent *f = updatefollowplayer();
				if(!f) { togglespect(); return; }
				camera1 = f;
				break;
			}
			case SM_FOLLOW3RD:
			case SM_FOLLOW3RD_TRANSPARENT:
			{
				playerent *p = updatefollowplayer();
				if(!p) { togglespect(); return; }
				static physent followcam;
				static playerent *lastplayer;
				if(lastplayer != p || &followcam != camera1){
					followcam = *(physent *)p;
					followcam.type = ENT_CAMERA;
					followcam.reset();
					followcam.roll = 0;
					followcam.move = -1;
					lastplayer = p;
					camera1 = &followcam;
				}
				followcam.o = p->o;

				// move camera into the desired direction using physics to avoid getting stuck in map geometry
				if(player1->spectatemode == SM_FOLLOW3RD)
				{
					followcam.vel.x = -(float)(cosf(RAD*(p->yaw-90)))*p->radius;
					followcam.vel.y = -(float)(sinf(RAD*(p->yaw-90)))*p->radius;
					followcam.vel.z = p->eyeheight;
				}
				else followcam.vel.z = p->eyeheight/6.0f;
				loopi(5) moveplayer(&followcam, 20, true, 5);
				followcam.vel.x = followcam.vel.y = followcam.vel.z = 0.0f;
				followcam.yaw = p->yaw;
				break;
			}

			default:
				resetcamera();
				break;
		}
	}else{
		resetcamera();
	}
}

void transplayer(){
	glLoadIdentity();

	glRotatef(camera1->roll, 0, 0, 1);
	glRotatef(camera1->pitch, -1, 0, 0);
	glRotatef(camera1->yaw, 0, 1, 0);

	// move from RH to Z-up LH quake style worldspace
	glRotatef(-90, 1, 0, 0);
	glScalef(1, -1, 1);

	glTranslatef(-camera1->o.x, -camera1->o.y, -camera1->o.z);
}

glmatrixf clipmatrix;

void genclipmatrix(float a, float b, float c, float d){
	// transform the clip plane into camera space
	float clip[4];
	loopi(4) clip[i] = a*invmvmatrix[i*4 + 0] + b*invmvmatrix[i*4 + 1] + c*invmvmatrix[i*4 + 2] + d*invmvmatrix[i*4 + 3];

	float x = ((clip[0]<0 ? -1 : (clip[0]>0 ? 1 : 0)) + projmatrix[8]) / projmatrix[0],
		  y = ((clip[1]<0 ? -1 : (clip[1]>0 ? 1 : 0)) + projmatrix[9]) / projmatrix[5],
		  w = (1 + projmatrix[10]) / projmatrix[14],
		  scale = 2 / (x*clip[0] + y*clip[1] - clip[2] + w*clip[3]);
	clipmatrix = projmatrix;
	clipmatrix[2] = clip[0]*scale;
	clipmatrix[6] = clip[1]*scale;
	clipmatrix[10] = clip[2]*scale + 1.0f;
	clipmatrix[14] = clip[3]*scale;
}

bool reflecting = false, refracting = false;
GLuint reflecttex = 0, refracttex = 0;
int reflectlastsize = 0;

VARP(reflectsize, 6, 8, 10);
VAR(reflectclip, 0, 3, 100);
VARP(waterreflect, 0, 1, 1);
VARP(waterrefract, 0, 0, 1);
VAR(reflectscissor, 0, 1, 1);

void drawreflection(float hf, int w, int h, float changelod, bool refract){
	reflecting = true;
	refracting = refract;

	int size = 1<<reflectsize, sizelimit = min(hwtexsize, min(w, h));
	while(size > sizelimit) size /= 2;
	if(size!=reflectlastsize){
		if(reflecttex) glDeleteTextures(1, &reflecttex);
		if(refracttex) glDeleteTextures(1, &refracttex);
		reflecttex = refracttex = 0;
	}
	if(!reflecttex || (waterrefract && !refracttex)){
		if(!reflecttex){
			glGenTextures(1, &reflecttex);
			createtexture(reflecttex, size, size, NULL, 3, false, GL_RGB);
		}
		if(!refracttex){
			glGenTextures(1, &refracttex);
			createtexture(refracttex, size, size, NULL, 3, false, GL_RGB);
		}
		reflectlastsize = size;
	}

	extern float wsx1, wsx2, wsy1, wsy2;
	int sx = 0, sy = 0, sw = size, sh = size;
	bool scissor = reflectscissor && (wsx1 > -1 || wsy1 > -1 || wsx1 < 1 || wsy1 < 1);
	if(scissor){
		sx = int(floor((wsx1+1)*0.5f*size));
		sy = int(floor((wsy1+1)*0.5f*size));
		sw = int(ceil((wsx2+1)*0.5f*size)) - sx;
		sh = int(ceil((wsy2+1)*0.5f*size)) - sy;
		glScissor(sx, sy, sw, sh);
		glEnable(GL_SCISSOR_TEST);
	}

	resetcubes();

	render_world(camera1->o.x, camera1->o.y, refract ? camera1->o.z : hf, changelod,
			(int)camera1->yaw, (refract ? 1 : -1)*(int)camera1->pitch, dynfov(), fovy, size, size);

	setupstrips();

	if(!refract) glCullFace(GL_BACK);
	glViewport(0, 0, size, size);
	glClear(GL_DEPTH_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);

	glPushMatrix();
	if(!refract){
		glTranslatef(0, 0, 2*hf);
		glScalef(1, 1, -1);
	}

	genclipmatrix(0, 0, -1, 0.1f*reflectclip+hf);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(clipmatrix.v);
	glMatrixMode(GL_MODELVIEW);

	renderstripssky();

	glPushMatrix();
	glLoadIdentity();
	glRotatef(camera1->pitch, -1, 0, 0);
	glRotatef(camera1->yaw,   0, 1, 0);
	glRotatef(90, 1, 0, 0);
	if(!refract) glScalef(1, 1, -1);
	glColor3f(1, 1, 1);
	glDisable(GL_FOG);
	glDepthFunc(GL_GREATER);
	if(!refracting) skyfloor = max(skyfloor, hf);
	draw_envbox(fog*4/3);
	glDepthFunc(GL_LESS);
	glEnable(GL_FOG);
	glPopMatrix();

	setuptmu(0, "T * P x 2");

	renderstrips();
	rendermapmodels();
	renderentities();
	renderclients();

	resettmu(0);

	render_particles(-1);

	if(refract) glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	extern int mtwater;
	if(refract && (!mtwater || maxtmus<2)){
		glLoadIdentity();
		glOrtho(0, 1, 0, 1, -1, 1);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ubv(hdr.watercolor);
		glBegin(GL_QUADS);
		glVertex2f(0, 1);
		glVertex2f(1, 1);
		glVertex2f(1, 0);
		glVertex2f(0, 0);
		glEnd();
		glDisable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_DEPTH_TEST);
	}
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if(!refract) glCullFace(GL_FRONT);
	glViewport(0, 0, w, h);

	if(scissor) glDisable(GL_SCISSOR_TEST);

	glBindTexture(GL_TEXTURE_2D, refract ? refracttex : reflecttex);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, sx, sy, sx, sy, sw, sh);

	glDisable(GL_TEXTURE_2D);

	reflecting = refracting = false;
}

bool minimap = false, minimapdirty = true;
int minimaplastsize = 0;
GLuint minimaptex = 0;

void clearminimap(){
	minimapdirty = true;
}

COMMAND(clearminimap, ARG_NONE);
VARFP(minimapres, 7, 9, 10, clearminimap());

void drawminimap(int w, int h){
	if(!minimapdirty) return;

	int size = 1<<minimapres, sizelimit = min(hwtexsize, min(w, h));
	while(size > sizelimit) size /= 2;
	if(size!=minimaplastsize && minimaptex){
		glDeleteTextures(1, &minimaptex);
		minimaptex = 0;
	}
	if(!minimaptex){
		glGenTextures(1, &minimaptex);
		createtexture(minimaptex, size, size, NULL, 3, false, GL_RGB);
		minimaplastsize = size;
	}

	minimap = true;

	disableraytable();

	physent *oldcam = camera1;
	physent minicam;
	camera1 = &minicam;
	camera1->type = ENT_CAMERA;
	camera1->o.x = camera1->o.y = ssize/2;
	camera1->o.z = 128;
	camera1->pitch = -90;
	camera1->yaw = 0;

	glViewport(0, 0, size, size);
	glClearDepth(0.0);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-ssize/2, ssize/2, -ssize/2, ssize/2, 0, 256);
	glScalef(1, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	glCullFace(GL_BACK);
	glDisable(GL_FOG);
	glEnable(GL_TEXTURE_2D);

	transplayer();

	resetcubes();

	render_world(camera1->o.x, camera1->o.y, camera1->o.z, 1.0f,
			(int)camera1->yaw, (int)camera1->pitch, 90.0f, 90.0f, size, size);

	setupstrips();

	setuptmu(0, "T * P x 2");
	glDepthFunc(GL_ALWAYS);
	renderstrips();
	glDepthFunc(GL_LESS);
	rendermapmodels();
	renderentities();
	resettmu(0);

	float hf = hdr.waterlevel-0.3f;
	renderwater(hf, 0, 0);

	camera1 = oldcam;
	minimap = false;

	glBindTexture(GL_TEXTURE_2D, minimaptex);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
	minimapdirty = false;

	glCullFace(GL_FRONT);
	glEnable(GL_FOG);
	glDisable(GL_TEXTURE_2D);

	glViewport(0, 0, w, h);
	glClearDepth(1.0);
}

void cleanupgl(){
	if(reflecttex) glDeleteTextures(1, &reflecttex);
	if(refracttex) glDeleteTextures(1, &refracttex);
	if(minimaptex) glDeleteTextures(1, &minimaptex);
	reflecttex = refracttex = minimaptex = 0;
	minimapdirty = true;

	if(glIsEnabled(GL_MULTISAMPLE)) glDisable(GL_MULTISAMPLE);
}

int xtraverts;

VARP(hudgun, 0, 1, 1);

void setperspective(float fovy, float nearplane){
	GLdouble ydist = nearplane * tan(fovy/2*RAD), xdist = ydist * aspect;

	float adsmax = 864, zoomf = (float)adszoom;
	if((gamefocus->weaponsel->type == GUN_SNIPER || gamefocus->weaponsel->type == GUN_BOLT) && gamefocus->ads){
		adsmax = sniperrifle::adsscope;
		zoomf = (float)scopezoom;
	}
	xdist *= 100 / (min(player1->ads/adsmax,1.f) * zoomf + 100);
	ydist *= 100 / (min(player1->ads/adsmax,1.f) * zoomf + 100);

	glFrustum(-xdist, xdist, -ydist, ydist, nearplane, farplane);
}

void sethudgunperspective(bool on){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if(on){
		glScalef(1, 1, 0.5f); // fix hudguns colliding with map geometry
		setperspective(75.0f, 0.01f); // y fov fixed at 75 degrees
	}
	else setperspective(fovy, 0.15f);
	glMatrixMode(GL_MODELVIEW);
}

void drawhudgun(int w, int h, float aspect, int farplane){
	sethudgunperspective(true);

	if(hudgun && camera1->type==ENT_PLAYER)
	{
		playerent *p = (playerent *)camera1;
		if(p->state==CS_ALIVE) p->weaponsel->renderhudmodel();
	}
	rendermenumdl();

	sethudgunperspective(false);
}

bool outsidemap(physent *pl){
	if(pl->o.x < 0 || pl->o.x >= ssize || pl->o.y <0 || pl->o.y > ssize) return true;
	sqr *s = S((int)pl->o.x, (int)pl->o.y);
	return SOLID(s)
		|| pl->o.z < s->floor - (s->type==FHF ? s->vdelta/4 : 0)
		|| pl->o.z > s->ceil  + (s->type==CHF ? s->vdelta/4 : 0);
}

float cursordepth = 0.9f;
glmatrixf mvmatrix, projmatrix, mvpmatrix, invmvmatrix, invmvpmatrix;
vec worldpos, camdir, camup, camright;

void readmatrices(){
	glGetFloatv(GL_MODELVIEW_MATRIX, mvmatrix.v);
	glGetFloatv(GL_PROJECTION_MATRIX, projmatrix.v);
	camright = vec(mvmatrix[0], mvmatrix[4], mvmatrix[8]);
	camup = vec(mvmatrix[1], mvmatrix[5], mvmatrix[9]);
	camdir = vec(-mvmatrix[2], -mvmatrix[6], -mvmatrix[10]);

	mvpmatrix.mul(projmatrix, mvmatrix);
	invmvmatrix.invert(mvmatrix);
	invmvpmatrix.invert(mvpmatrix);
}

inline void traceShot(const vec &from, vec &to, dynent *tracer, float len){
	traceresult_s tr;
	to.sub(from).normalize().mul(len).add(from);
	TraceLine(from, to, tracer, false, &tr);
	to = tr.end;
}

// stupid function to cater for stupid ATI linux drivers that return incorrect depth values

inline float depthcorrect(float d){
	return (d<=1/256.0f) ? d*256 : d;
}

// find out the 3d target of the crosshair in the world easily and very acurately.
// sadly many very old cards and drivers appear to fuck up on glReadPixels() and give false
// coordinates, making shooting and such impossible.
// NO LONGER hits map entities which is GOOD

void readdepth(int w, int h, vec &pos){
	glReadPixels(w/2, h/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &cursordepth);
	vec screen(0, 0, depthcorrect(cursordepth)*2 - 1);
	vec4 world;
	invmvpmatrix.transform(screen, world);
	pos = vec(world.x, world.y, world.z).div(world.w);
	traceShot(camera1->o, pos);
}

void gl_drawframe(int w, int h, float changelod, float curfps){
	dodynlights();
	drawminimap(w, h);

	recomputecamera();

	aspect = float(w)/h;
	fovy = 2*atan2(tan(float(dynfov())/2*RAD), aspect)/RAD;

	float hf = hdr.waterlevel-0.3f;
	const bool underwater = camera1->o.z<hf/*, flashed = gamefocus->flashmillis > 0 && lastmillis <= gamefocus->flashmillis*/;

	glFogi(GL_FOG_START, (fog+64)/8);
	glFogi(GL_FOG_END, fog);
	float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f },
		  wfogc[4] = { hdr.watercolor[0]/255.0f, hdr.watercolor[1]/255.0f, hdr.watercolor[2]/255.0f, 1.0f };
	glFogfv(GL_FOG_COLOR, fogc);
	glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if(underwater){
		fovy += sinf(lastmillis/1000.0f)*2.0f;
		aspect += sinf(lastmillis/1000.0f+PI)*0.1f;
		glFogfv(GL_FOG_COLOR, wfogc);
		glFogi(GL_FOG_START, 0);
		glFogi(GL_FOG_END, (fog+96)/8);
	}

	farplane = fog*5/2;
	setperspective(fovy, 0.15f);
	glMatrixMode(GL_MODELVIEW);

	transplayer();
	readmatrices();

	if(!underwater && waterreflect)
	{
		extern int wx1;
		if(wx1>=0)
		{
			if(reflectscissor) calcwaterscissor();
			drawreflection(hf, w, h, changelod, false);
			if(waterrefract) drawreflection(hf, w, h, changelod, true);
		}
	}

	if(stencilshadow && hasstencil && stencilbits >= 8) glClearStencil((hasSTS || hasST2) && !hasSTW ? 128 : 0);
	glClear((outsidemap(camera1) ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT | (stencilshadow && hasstencil && stencilbits >= 8 ? GL_STENCIL_BUFFER_BIT : 0));

	glEnable(GL_TEXTURE_2D);

	resetcubes();

	render_world(camera1->o.x, camera1->o.y, camera1->o.z, changelod,
			(int)camera1->yaw, (int)camera1->pitch, dynfov(), fovy, w, h);

	setupstrips();

	renderstripssky();

	glLoadIdentity();
	glRotatef(camera1->pitch, -1, 0, 0);
	glRotatef(camera1->yaw,   0, 1, 0);
	glRotatef(90, 1, 0, 0);
	glColor3f(1, 1, 1);
	glDisable(GL_FOG);
	glDepthFunc(GL_GREATER);
	draw_envbox(fog*4/3);
	glDepthFunc(GL_LESS);
	fixresizedscreen();
	glEnable(GL_FOG);

	transplayer();

	setuptmu(0, "T * P x 2");

	renderstrips();

	xtraverts = 0;

	startmodelbatches();
	rendermapmodels();
	endmodelbatches();

	if(stencilshadow && hasstencil && stencilbits >= 8) drawstencilshadows();

	startmodelbatches();
	renderentities();
	endmodelbatches();

	readdepth(w, h, worldpos);

	startmodelbatches();
	renderclients();
	endmodelbatches();

	startmodelbatches();
	renderbounceents();
	endmodelbatches();

	// Added by Rick: Need todo here because of drawing the waypoints
	WaypointClass.Think();
	// end add

	drawhudgun(w, h, aspect, farplane);

	resettmu(0);

	glDisable(GL_CULL_FACE);

	render_particles(curtime, PT_DECAL_MASK);

	int nquads = renderwater(hf, !waterreflect || underwater ? 0 : reflecttex, !waterreflect || !waterrefract || underwater ? 0 : refracttex);

	render_particles(curtime, ~PT_DECAL_MASK);

	glDisable(GL_FOG);
	glDisable(GL_TEXTURE_2D);

	if(editmode)
	{
		if(cursordepth==1.0f) worldpos = camera1->o;
		enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDepthMask(GL_FALSE);
		cursorupdate();
		glDepthMask(GL_TRUE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		disablepolygonoffset(GL_POLYGON_OFFSET_LINE, false);
	}

	extern vector<vertex> verts;
	gl_drawhud(w, h, (int)round(curfps), nquads, verts.length(), underwater);

	glEnable(GL_CULL_FACE);
	glEnable(GL_FOG);

	undodynlights();
}

