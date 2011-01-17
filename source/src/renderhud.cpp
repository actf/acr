// renderhud.cpp: HUD rendering

#include "pch.h"
#include "cube.h"

void drawicon(Texture *tex, float x, float y, float s, int col, int row, float ts)
{
	if(tex && tex->xs == tex->ys) quad(tex->id, x, y, s, ts*col, ts*row, ts);
}

void drawequipicon(float x, float y, int col, int row, int pulse, playerent *p = NULL) // pulse: 3 = (1 - pulse) | (2 - blue regen)
{
	static Texture *tex = NULL;
	if(!tex) tex = textureload("packages/misc/items.png", 4);
	if(tex)
	{
		glEnable(GL_BLEND);
		float antiblue = (pulse&2) && p ? (lastmillis-p->lastregen)/1000.f : 1.f;
		glColor4f(antiblue, antiblue, antiblue*2, (pulse&1) ? (0.2f+(sinf(lastmillis/100.0f)+1.0f)/2.0f) : 1.f);
		drawicon(tex, x, y, 120, col, row, 1/4.0f);
		glDisable(GL_BLEND);
	}
}

void drawradaricon(float x, float y, float s, int col, int row)
{
	static Texture *tex = NULL;
	if(!tex) tex = textureload("packages/misc/radaricons.png", 3);
	if(tex)
	{
		glEnable(GL_BLEND);
		drawicon(tex, x, y, s, col, row, 1/4.0f);
		glDisable(GL_BLEND);
	}
}

void drawctficon(float x, float y, float s, int col, int row, float ts)
{
	static Texture *ctftex = NULL, *htftex = NULL, *ktftex = NULL;
	if(!ctftex) ctftex = textureload("packages/misc/ctficons.png", 3);
	if(!htftex) htftex = textureload("packages/misc/htficons.png", 3);
	if(!ktftex) ktftex = textureload("packages/misc/ktficons.png", 3);
	if(m_htf)
	{
		if(htftex) drawicon(htftex, x, y, s, col, row, ts);
	}
	else if(m_ktf)
	{
		if(ktftex) drawicon(ktftex, x, y, s, col, row, ts);
	}
	else
	{
		if(ctftex) drawicon(ctftex, x, y, s, col, row, ts);
	}
}

void drawvoteicon(float x, float y, int col, int row, bool noblend)
{
	static Texture *tex = NULL;
	if(!tex) tex = textureload("packages/misc/voteicons.png", 3);
	if(tex)
	{
		if(noblend) glDisable(GL_BLEND);
		drawicon(tex, x, y, 240, col, row, 1/2.0f);
		if(noblend) glEnable(GL_BLEND);
	}
}

VARP(crosshairsize, 0, 15, 50);
VARP(hidestats, 0, 1, 1);
VARP(hideradar, 0, 0, 1);
VARP(hidecompass, 0, 0, 1);
VARP(hideteam, 0, 0, 1);
//VARP(radarres, 1, 64, 1024); // auto
VARP(radarentsize, 1, 4, 64);
VARP(hidectfhud, 0, 0, 1);
VARP(hidevote, 0, 0, 2);
VARP(hidehudmsgs, 0, 0, 1);
VARP(hidehudequipment, 0, 0, 1);
VARP(hideconsole, 0, 0, 1);
VARP(hidespecthud, 0, 0, 1);
VAR(showmap, 0, 0, 1);

void drawscope()
{
	// this may need to change depending on the aspect ratio at which the scope image is drawn at
	const float scopeaspect = 4.0f/3.0f;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	static Texture *scopetex = NULL;
	if(!scopetex) scopetex = textureload("packages/misc/scope.png", 3);
	glBindTexture(GL_TEXTURE_2D, scopetex->id);
	glBegin(GL_QUADS);
	glColor3ub(255,255,255);

	// figure out the bounds of the scope given the desired aspect ratio
	float w = min(scopeaspect*VIRTH, float(VIRTW)),
		  x1 = VIRTW/2 - w/2,
		  x2 = VIRTW/2 + w/2;

	glTexCoord2f(0, 0); glVertex2f(x1, 0);
	glTexCoord2f(1, 0); glVertex2f(x2, 0);
	glTexCoord2f(1, 1); glVertex2f(x2, VIRTH);
	glTexCoord2f(0, 1); glVertex2f(x1, VIRTH);

	// fill unused space with border texels
	if(x1 > 0)
	{
		glTexCoord2f(0, 0); glVertex2f(0, 0);
		glTexCoord2f(0, 0); glVertex2f(x1, 0);
		glTexCoord2f(0, 1); glVertex2f(x1, VIRTH);
		glTexCoord2f(0, 1); glVertex2f(0, VIRTH);
	}

	if(x2 < VIRTW)
	{
		glTexCoord2f(1, 0); glVertex2f(x2, 0);
		glTexCoord2f(1, 0); glVertex2f(VIRTW, 0);
		glTexCoord2f(1, 1); glVertex2f(VIRTW, VIRTH);
		glTexCoord2f(1, 1); glVertex2f(x2, VIRTH);
	}

	glEnd();
}

const char *crosshairnames[CROSSHAIR_NUM] = { "default", "scope", "shotgun", "vertical", "horizontal" };
Texture *crosshairs[CROSSHAIR_NUM] = { NULL }; // weapon specific crosshairs

Texture *loadcrosshairtexture(const char *c, int type = -1)
{
	s_sprintfd(p)("packages/misc/crosshairs/%s", c);
	Texture *crosshair = textureload(p, 3);
	if(crosshair==notexture){
		s_sprintf(p)("packages/misc/crosshairs/%s", crosshairnames[type < 0 || type >= CROSSHAIR_NUM ? CROSSHAIR_DEFAULT : type]);
		crosshair = textureload(p, 3);
	}
	return crosshair;
}

void loadcrosshair(char *c, char *name)
{
	int n = -1;
	loopi(CROSSHAIR_NUM) if(!strcmp(crosshairnames[i], name)) { n = i; break; }
	if(n<0){
		n = atoi(name);
		if(n<0 || n>=CROSSHAIR_NUM) return;
	}
	crosshairs[n] = loadcrosshairtexture(c, n);
}

COMMAND(loadcrosshair, ARG_2STR);

void drawcrosshair(playerent *p, int n, int teamtype, color *c, float size)
{
	Texture *crosshair = crosshairs[n];
	if(!crosshair)
	{
		crosshair = crosshairs[CROSSHAIR_DEFAULT];
		if(!crosshair) crosshair = crosshairs[CROSSHAIR_DEFAULT] = loadcrosshairtexture("default.png");
	}

	if(crosshair->bpp==32) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glBindTexture(GL_TEXTURE_2D, crosshair->id);
	static color col;
	col.r = col.b = col.g = col.alpha = 1.f;
	if(c) col = color(c->r, c->g, c->b);
	else if(teamtype)
	{
		if(teamtype == 1) col = color(0.f, 1.f, 0.f);
		else if(teamtype == 2) col = color(1.f, 0.f, 0.f);
	}
	else if(!m_osok){
		if(p->health<=50) col = color(0.5f, 0.25f, 0.f); // orange-red
		if(p->health<=25) col = color(0.5f, 0.125f, 0.f); // red-orange
	}
	if(n == CROSSHAIR_DEFAULT) col.alpha = 1.f + p->weaponsel->dynspread() / -1200.f;
	glColor4f(col.r, col.g, col.b, col.alpha);
	float usz = (float)crosshairsize, chsize = size>0 ? size : usz;
	if(n == CROSSHAIR_DEFAULT){
		usz *= 3.5f;
		float ct = usz / 1.8f;
		chsize = p->weaponsel->dynspread() * 100 / dynfov();
		Texture *cv = crosshairs[CROSSHAIR_V], *ch = crosshairs[CROSSHAIR_H];
		if(!cv) cv = textureload("packages/misc/crosshairs/vertical.png", 3);
		if(!ch) ch = textureload("packages/misc/crosshairs/horizontal.png", 3);
		glBindTexture(GL_TEXTURE_2D, ch->id);
		glBegin(GL_QUADS);
		// top
		glTexCoord2f(0, 0); glVertex2f(VIRTW/2 - ct, VIRTH/2 - chsize - usz);
		glTexCoord2f(1, 0); glVertex2f(VIRTW/2 + ct, VIRTH/2 - chsize - usz);
		glTexCoord2f(1, 1); glVertex2f(VIRTW/2 + ct, VIRTH/2 - chsize);
		glTexCoord2f(0, 1); glVertex2f(VIRTW/2 - ct, VIRTH/2 - chsize);
		// bottom
		glTexCoord2f(0, 0); glVertex2f(VIRTW/2 - ct, VIRTH/2 + chsize);
		glTexCoord2f(1, 0); glVertex2f(VIRTW/2 + ct, VIRTH/2 + chsize);
		glTexCoord2f(1, 1); glVertex2f(VIRTW/2 + ct, VIRTH/2 + chsize + usz);
		glTexCoord2f(0, 1); glVertex2f(VIRTW/2 - ct, VIRTH/2 + chsize + usz);
		// change texture
		glEnd();
		glBindTexture(GL_TEXTURE_2D, cv->id);
		glBegin(GL_QUADS);
		// left
		glTexCoord2f(0, 0); glVertex2f(VIRTW/2 - chsize - usz, VIRTH/2 - ct);
		glTexCoord2f(1, 0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - ct);
		glTexCoord2f(1, 1); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + ct);
		glTexCoord2f(0, 1); glVertex2f(VIRTW/2 - chsize - usz, VIRTH/2 + ct);
		// right
		glTexCoord2f(0, 0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - ct);
		glTexCoord2f(1, 0); glVertex2f(VIRTW/2 + chsize + usz, VIRTH/2 - ct);
		glTexCoord2f(1, 1); glVertex2f(VIRTW/2 + chsize + usz, VIRTH/2 + ct);
		glTexCoord2f(0, 1); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + ct);
	}
	else{
	if(n == CROSSHAIR_SHOTGUN) chsize = SGSPREAD * 100 / dynfov();
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
		glTexCoord2f(1, 0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
		glTexCoord2f(1, 1); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
		glTexCoord2f(0, 1); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
	}
	glEnd();
}

VARP(hidedamageindicator, 0, 0, 1);
VARP(damageindicatorsize, 0, 200, 10000);
VARP(damageindicatordist, 0, 500, 10000);
VARP(damageindicatortime, 1, 1000, 10000);
VARP(damageindicatoralpha, 1, 50, 100);
int damagedirections[4] = {0};

void updatedmgindicator(vec &attack)
{
	if(hidedamageindicator || !damageindicatorsize) return;
	float bestdist = 0.0f;
	int bestdir = -1;
	loopi(4)
	{
		vec d;
		d.x = (float)(cosf(RAD*(player1->yaw-90+(i*90))));
		d.y = (float)(sinf(RAD*(player1->yaw-90+(i*90))));
		d.z = 0.0f;
		d.add(player1->o);
		float dist = d.dist(attack);
		if(dist < bestdist || bestdir==-1)
		{
			bestdist = dist;
			bestdir = i;
		}
	}
	damagedirections[bestdir] = lastmillis+damageindicatortime;
}

void drawdmgindicator()
{
	if(!damageindicatorsize) return;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	float size = (float)damageindicatorsize;
	loopi(4)
	{
		if(!damagedirections[i] || damagedirections[i] < lastmillis) continue;
		float t = damageindicatorsize/(float)(damagedirections[i]-lastmillis);
		glPushMatrix();
		glColor4f(0.5f, 0.0f, 0.0f, damageindicatoralpha/100.0f);
		glTranslatef(VIRTW/2, VIRTH/2, 0);
		glRotatef(i*90, 0, 0, 1);
		glTranslatef(0, (float)-damageindicatordist, 0);
		glScalef(max(0.0f, 1.0f-t), max(0.0f, 1.0f-t), 0);

		glBegin(GL_TRIANGLES);
		glVertex3f(size/2.0f, size/2.0f, 0.0f);
		glVertex3f(-size/2.0f, size/2.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glEnd();
		glPopMatrix();
	}
	glEnable(GL_TEXTURE_2D);
}

void drawequipicons(playerent *p)
{
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// health & armor
	if(p->armour)
		if(p->armour > 25) drawequipicon(560, 1650, (p->armour - 25) / 25, 2, 0);
		else drawequipicon(560, 1650, 3, 3, 0);
	drawequipicon(20, 1650, 2, 3, (lastmillis - p->lastregen < 1000 ? 2 : 0) | ((p->state!=CS_DEAD && p->health<=35 && !m_osok) ? 1 : 0), p);
	if(p->mag[GUN_GRENADE]) drawequipicon(1520, 1650, 3, 1, 0);

	// weapons
	int c = p->weaponsel->type, r = 0;
	if(c == GUN_GRENADE){// draw nades separately
		if(p->prevweaponsel->type != GUN_GRENADE) c = p->prevweaponsel->type;
		else if(p->nextweaponsel->type != GUN_GRENADE) c = p->nextweaponsel->type;
		else c = 14; // unknown = HP symbol
	}
	else if(c == GUN_AKIMBO) c = GUN_PISTOL; // same icon for akimbo & pistol
	switch(c){
		case GUN_KNIFE: case GUN_PISTOL: default: break; // aligned properly
		case GUN_SHOTGUN: c = 2; break;
		case GUN_SUBGUN: c = 4; break;
		case GUN_SNIPER: c = 5; break;
		case GUN_SLUG: c = 3; break;
		case GUN_ASSAULT: c = 6; break;
	}
	if(c > 3) { c -= 4; r = 1; }

	if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS)
		drawequipicon(1020, 1650, c, r, ((!p->weaponsel->ammo || p->weaponsel->mag < magsize(p->weaponsel->type) / 3) && p->weaponsel->type != GUN_KNIFE && p->weaponsel->type != GUN_GRENADE) ? 1 : 0);
	glEnable(GL_BLEND);
}

void drawradarent(const vec &o, float coordtrans, float yaw, int col, int row, float iconsize, bool pulse, float alpha = 1.f, const char *label = NULL, ...)
{
	glPushMatrix();
	if(pulse) glColor4f(1.0f, 1.0f, 1.0f, 0.2f+(sinf(lastmillis/30.0f)+1.0f)/2.0f);
	else glColor4f(1, 1, 1, alpha);
	glTranslatef(o.x * coordtrans, o.y * coordtrans, 0);
	glRotatef(yaw, 0, 0, 1);
	drawradaricon(-iconsize/2.0f, -iconsize/2.0f, iconsize, col, row);
	glPopMatrix();
	if(label && showmap)
	{
		glPushMatrix();
		glEnable(GL_BLEND);
		glTranslatef(iconsize/2, iconsize/2, 0);
		glScalef(1/2.0f, 1/2.0f, 1/2.0f);
		s_sprintfdv(lbl, label);
		draw_text(lbl, (int)(o.x * coordtrans *2), (int)(o.y * coordtrans*2));
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
		glPopMatrix();
	}
}

struct hudline : cline
{
	int type;

	hudline() : type(HUDMSG_INFO) {}
};

struct hudmessages : consolebuffer<hudline>
{
	hudmessages() : consolebuffer<hudline>(20) {}

	void addline(const char *sf)
	{
		if(conlines.length() && conlines[0].type&HUDMSG_OVERWRITE)
		{
			conlines[0].millis = totalmillis;
			conlines[0].type = HUDMSG_INFO;
			s_strcpy(conlines[0].line, sf);
		}
		else consolebuffer<hudline>::addline(sf, totalmillis);
	}
	void editline(int type, const char *sf)
	{
		if(conlines.length() && ((conlines[0].type&HUDMSG_TYPE)==(type&HUDMSG_TYPE) || conlines[0].type&HUDMSG_OVERWRITE))
		{
			conlines[0].millis = totalmillis;
			conlines[0].type = type;
			s_strcpy(conlines[0].line, sf);
		}
		else consolebuffer<hudline>::addline(sf, totalmillis).type = type;
	}
	void render()
	{
		if(!conlines.length()) return;
		glLoadIdentity();
		glOrtho(0, VIRTW*0.9f, VIRTH*0.9f, 0, -1, 1);
		int dispmillis = arenaintermission ? 6000 : 3000;
		loopi(min(conlines.length(), 3)) if(totalmillis-conlines[i].millis<dispmillis)
		{
			cline &c = conlines[i];
			int tw = text_width(c.line);
			draw_text(c.line, int(tw > VIRTW*0.9f ? 0 : (VIRTW*0.9f-tw)/2), int(((VIRTH*0.9f)/4*3)+FONTH*i+pow((totalmillis-c.millis)/(float)dispmillis, 4)*VIRTH*0.9f/4.0f));
		}
	}
};

hudmessages hudmsgs;

void hudoutf(const char *s, ...)
{
	s_sprintfdv(sf, s);
	hudmsgs.addline(sf);
	conoutf("%s", sf);
}

void hudonlyf(const char *s, ...)
{
	s_sprintfdv(sf, s);
	hudmsgs.addline(sf);
}

void hudeditf(int type, const char *s, ...)
{
	s_sprintfdv(sf, s);
	hudmsgs.editline(type, sf);
}

bool insideradar(const vec &centerpos, float radius, const vec &o)
{
	if(showmap) return !o.reject(centerpos, radius);
	return o.distxy(centerpos)<=radius;
}

bool isattacking(playerent *p) { return lastmillis-p->lastaction < 500; }

vec getradarpos()
{
	float radarviewsize = VIRTH/6;
	float overlaysize = radarviewsize*4.0f/3.25f;
	return vec(VIRTW-10-VIRTH/28-overlaysize, 10+VIRTH/52, 0);
}

VARP(radarenemyfade, 0, 1250, 1250);

// DrawCircle from http://slabode.exofire.net/circle_draw.shtmls (public domain, but I'll reference it anyways
#define circleSegments 720
void DrawCircle(float cx, float cy, float r, float coordmul, int *col, float thickness = 1.f, float opacity = 250 / 255.f){
	float theta = 2 * 3.1415926 / float(circleSegments); 
	float c = cosf(theta); // precalculate the sine and cosine
	float s = sinf(theta);
	float t;

	float x = r * coordmul;// we start at angle = 0 
	float y = 0; 
    
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glLineWidth(thickness);
	glBegin(GL_LINE_LOOP);
	glColor4f(col[0] / 255.f, col[1] / 255.f, col[2] / 255.f, opacity);
	for(int ii = 0; ii < circleSegments; ii++){
		glVertex2f(x + cx * coordmul, y + cy * coordmul); // output vertex
		// apply the rotation matrix
		t = x;
		x = c * x - s * y;
		y = s * t + c * y;
	}
	glEnd(); 
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

float easedradarsize = 64;
void drawradar(playerent *p, int w, int h)
{
	vec center = showmap ? vec(ssize/2, ssize/2, 0) : p->o;
	easedradarsize = clamp(((easedradarsize * 60.f + p->o.dist(worldpos) * 3.f) / 61.f), 50.f, ssize/2.f); // 2.5f is normal scaling; 3 for extra view
	float res = showmap ? ssize : easedradarsize;

	float worldsize = (float)ssize;
	float radarviewsize = showmap ? VIRTH : VIRTH/6;
	float radarsize = worldsize/res*radarviewsize;
	float iconsize = radarentsize/res*radarviewsize;
	float coordtrans = radarsize/worldsize;
	float overlaysize = radarviewsize*4.0f/3.25f;

	glColor3f(1.0f, 1.0f, 1.0f);
	glPushMatrix();

	if(showmap) glTranslatef(VIRTW/2-radarviewsize/2, 0, 0);
	else
	{
		glTranslatef(VIRTW-VIRTH/28-radarviewsize-(overlaysize-radarviewsize)/2-10+radarviewsize/2, 10+VIRTH/52+(overlaysize-radarviewsize)/2+radarviewsize/2, 0);
		glRotatef(-camera1->yaw, 0, 0, 1);
		glTranslatef(-radarviewsize/2, -radarviewsize/2, 0);
	}

	extern GLuint minimaptex;

	vec centerpos(min(max(center.x, res/2.0f), worldsize-res/2.0f), min(max(center.y, res/2.0f), worldsize-res/2), 0.0f);
	if(showmap)
	{
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		quad(minimaptex, 0, 0, radarviewsize, (centerpos.x-res/2)/worldsize, (centerpos.y-res/2)/worldsize, res/worldsize);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
		circle(minimaptex, radarviewsize/2, radarviewsize/2, radarviewsize/2, centerpos.x/worldsize, centerpos.y/worldsize, res/2/worldsize);
	}
	glTranslatef(-(centerpos.x-res/2)/worldsize*radarsize, -(centerpos.y-res/2)/worldsize*radarsize, 0);

	drawradarent(p->o, coordtrans, p->yaw, p->state==CS_ALIVE ? (isattacking(p) ? 2 : 0) : 1, 2, iconsize, isattacking(p), 1.f, "\f1%s", colorname(p)); // local player

	loopv(players) // other players
	{
		playerent *pl = players[i];
		if(!pl || pl == p || !insideradar(centerpos, res/2, pl->o)) continue;
		bool hasflag = pl == flaginfos[0].actor || pl == flaginfos[1].actor;
		if(!hasflag && pl->state != CS_DEAD && !isteam(p, pl)){
			playerent *seenby = NULL;
			extern bool IsVisible(vec v1, vec v2, dynent *tracer = NULL, bool SkipTags=false);
			if(IsVisible(p->o, pl->o)) seenby = p;
			else loopvj(players){
				playerent *pll = players[j];
				if(!pll || pll == p || !isteam(p, pll) || pll->state == CS_DEAD) continue;
				if(IsVisible(pll->o, pl->o)) { seenby = pll; break;}
			}
			if(seenby){
				pl->radarmillis = lastmillis + (seenby == p ? 750 : 250);
				pl->lastloudpos[0] = pl->o.x;
				pl->lastloudpos[1] = pl->o.y;
				pl->lastloudpos[2] = pl->yaw;
			}
			else if(pl->radarmillis + radarenemyfade < lastmillis) continue;
		}
		if(isteam(p, pl) || hasflag || pl->state == CS_DEAD) // friendly, flag tracker or dead
			drawradarent(pl->o, coordtrans, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1,
				pl->team, iconsize, isattacking(pl), 1.f, "\f%d%s", isteam(p, pl) ? 0 : 3, colorname(pl));
		else
			drawradarent(pl->lastloudpos, coordtrans, pl->lastloudpos[2], pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1,
				pl->team, iconsize, false, (radarenemyfade - lastmillis + pl->radarmillis) / (float)radarenemyfade, "\f3%s", colorname(pl));
	}
	loopv(bounceents){ // draw grenades
		bounceent *b = bounceents[i];
		if(b == NULL || b->bouncetype != BT_NADE) continue;
		if(((grenadeent *)b)->nadestate != 1) continue;
		drawradarent(vec(b->o.x, b->o.y, 0), coordtrans, 0, b->owner == p ? 2 : isteam(b->owner, p) ? 1 : 0, 3, iconsize/1.5f, true);
	}
	int col[3] = {255, 255, 255};
	#define setcol(c1, c2, c3) col[0] = c1; col[1] = c2; col[2] = c3;
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	loopv(nxp){ // draw explosions
		int ndelay = lastmillis - nxp[i].millis;
		if(ndelay > 600) nxp.remove(i--);
		else{
			if(nxp[i].owner == p) {setcol(0xf7, 0xf5, 0x34)} // yellow for your explosions
			else if(isteam(p, nxp[i].owner)) {setcol(0x02, 0x13, 0xFB)} // blue for friendlies' explosions
			else {setcol(0xFB, 0x02, 0x02)} // red for enemies' explosions
			if(ndelay / 400.f < 1) DrawCircle(nxp[i].o[0], nxp[i].o[1], ndelay / 100.f, coordtrans, col, 2.f, 1 - ndelay / 400.f);
			DrawCircle(nxp[i].o[0], nxp[i].o[1], pow(ndelay, 1.5f) / 3094.0923f, coordtrans, col, 1.f, 1 - ndelay / 600.f);
		}
	}
	loopv(sls){ // shotlines
		if(sls[i].expire < lastmillis) sls.remove(i--);
		else{
			if(sls[i].owner == p) {setcol(0x94, 0xB0, 0xDE)} // blue for your shots
			else if(isteam(p, sls[i].owner)) {setcol(0xB8, 0xDC, 0x78)} // light green-yellow for friendlies
			else {setcol(0xFF, 0xFF, 0xFF)} // white for enemies
			glBegin(GL_LINES);
			// source shot
			glColor4ub(col[0], col[1], col[2], 200);
			glVertex2f(sls[i].from[0]*coordtrans, sls[i].from[1]*coordtrans);
			// dest shot
			glColor4ub(col[0], col[1], col[2], 250);
			glVertex2f(sls[i].to[0]*coordtrans, sls[i].to[1]*coordtrans);
			glEnd();
		}
	}
	glEnable(GL_TEXTURE_2D);

	if(m_flags)
	{
		glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		loopi(2) // flag items
		{
			flaginfo &f = flaginfos[i];
			entity *e = f.flagent;
			if(!e) continue;
			float yaw = showmap ? 0 : camera1->yaw;
			if(insideradar(centerpos, res/2, vec(e->x, e->y, centerpos.z)))
				drawradarent(vec(e->x, e->y, 0), coordtrans, yaw, m_ktf && f.state!=CTFF_IDLE ? 2 : f.team, 3, iconsize, false); // draw bases
			if(m_ktf && f.state==CTFF_IDLE) continue;
			vec pos(0.5f-0.1f, 0.5f-0.9f, 0);
			pos.mul(iconsize/coordtrans).rotate_around_z(yaw*RAD);
			if(f.state==CTFF_STOLEN)
			{
				if(f.actor)
				{
					pos.add(f.actor->o);
					// see flag position no matter what!
					if(insideradar(centerpos, res/2, pos))
						drawradarent(pos, coordtrans, yaw, 3, m_ktf ? 2 : f.team, iconsize, true); // draw near flag thief
				}
			}
			else
			{
				pos.x += e->x;
				pos.y += e->y;
				pos.z += centerpos.z;
				if(insideradar(centerpos, res/2, pos)) drawradarent(pos, coordtrans, yaw, 3, m_ktf ? 2 : f.team, iconsize, false); // draw on entitiy pos
			}
		}
	}

	glEnable(GL_BLEND);
	glPopMatrix();

	if(!showmap)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor3f(1, 1, 1);
		static Texture *bordertex = NULL;
		if(!bordertex) bordertex = textureload("packages/misc/compass-base.png", 3);
		quad(bordertex->id, VIRTW-10-VIRTH/28-overlaysize, 10+VIRTH/52, overlaysize, 0, 0, 1, 1);
		if(!hidecompass)
		{
			static Texture *compasstex = NULL;
			if(!compasstex) compasstex = textureload("packages/misc/compass-rose.png", 3);
			glPushMatrix();
			glTranslatef(VIRTW-10-VIRTH/28-overlaysize/2, 10+VIRTH/52+overlaysize/2, 0);
			glRotatef(-camera1->yaw, 0, 0, 1);
			quad(compasstex->id, -overlaysize/2, -overlaysize/2, overlaysize, 0, 0, 1, 1);
			glPopMatrix();
		}
	}
}

void drawteamicons(int w, int h)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor3f(1, 1, 1);
	static Texture *icons = NULL;
	if(!icons) icons = textureload("packages/misc/teamicons.png", 3);
	quad(icons->id, VIRTW-VIRTH/12-10, 10, VIRTH/12, player1->team ? 0.5f : 0, 0, 0.49f, 1.0f);
}

int damageblendmillis = 0;

VARFP(damagescreen, 0, 1, 1, { if(!damagescreen) damageblendmillis = 0; });
VARP(damagescreenfactor, 1, 7, 100);
VARP(damagescreenalpha, 1, 45, 100);
VARP(damagescreenfade, 0, 125, 1000);

void damageblend(int n)
{
	if(!damagescreen) return;
	if(lastmillis > damageblendmillis) damageblendmillis = lastmillis;
	damageblendmillis += min(n*damagescreenfactor, 1500);
}

static int votersort(playerent **a, playerent **b){
	return (*a)->voternum - (*b)->voternum;
}

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
	playerent *p = camera1->type<ENT_CAMERA ? (playerent *)camera1 : player1;
	bool spectating = player1->isspectating();

	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
	glEnable(GL_BLEND);

	if(underwater)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(hdr.watercolor[0], hdr.watercolor[1], hdr.watercolor[2], 102);

		glBegin(GL_QUADS);
		glVertex2f(0, 0);
		glVertex2f(VIRTW, 0);
		glVertex2f(VIRTW, VIRTH);
		glVertex2f(0, VIRTH);
		glEnd();
	}

	if(lastmillis < damageblendmillis)
	{
		static Texture *damagetex = NULL;
		if(!damagetex) damagetex = textureload("packages/misc/damage.png", 3);

		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, damagetex->id);
		float fade = damagescreenalpha/100.0f;
		if(damageblendmillis - lastmillis < damagescreenfade)
			fade *= float(damageblendmillis - lastmillis)/damagescreenfade;
		glColor4f(fade, fade, fade, fade);

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(0, 0);
		glTexCoord2f(1, 0); glVertex2f(VIRTW, 0);
		glTexCoord2f(1, 1); glVertex2f(VIRTW, VIRTH);
		glTexCoord2f(0, 1); glVertex2f(0, VIRTH);
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);

	int targetplayerzone = 0;
	playerent *targetplayer = playerincrosshairhit(targetplayerzone);
	bool menu = menuvisible();
	bool command = getcurcommand() ? true : false;
	if(!p->weaponsel->reloading && !p->weaponchanging){
		if(p->state==CS_ALIVE) p->weaponsel->renderaimhelp(targetplayer && targetplayer->state==CS_ALIVE ? isteam(targetplayer, p) ? 1 : 2 : 0);
		else if(p->state==CS_EDITING) 
			drawcrosshair(p, CROSSHAIR_SCOPE, targetplayer && targetplayer->state==CS_ALIVE ? isteam(targetplayer, p) ? 1 : 2 : 0, NULL, 48.f);
	}

	drawdmgindicator();

	static Texture *headshottex = NULL;
	if(!headshottex) headshottex = textureload("packages/misc/headshot.png", 3);
	if(lastmillis - p->lastheadshot < 3000){
		glBindTexture(GL_TEXTURE_2D, headshottex->id);
		glEnable(GL_BLEND);
		glColor4f(1.f, 1.f, 1.f, (3000 + p->lastheadshot - lastmillis) / 3000.f);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(VIRTW * 0.35f, VIRTH * 0.2f);
		glTexCoord2f(1, 0);	glVertex2f(VIRTW * 0.65f, VIRTH * 0.2f);
		glTexCoord2f(1, 1); glVertex2f(VIRTW * 0.65f, VIRTH * 0.4f);
		glTexCoord2f(0,	1); glVertex2f(VIRTW * 0.35f, VIRTH * 0.4f);
		glEnd();
	}

	if(p->state==CS_ALIVE && !hidehudequipment) drawequipicons(p);

	if(!editmode)
	{
		glMatrixMode(GL_MODELVIEW);
		if(/*!menu &&*/ (!hideradar || showmap)) drawradar(p, w, h);
		if(!hideteam && m_teammode) drawteamicons(w, h);
		glMatrixMode(GL_PROJECTION);
	}

	char *infostr = editinfo();
	int commandh = 1570 + FONTH;
	if(command) commandh -= rendercommand(20, 1570, VIRTW);
	else if(infostr) draw_text(infostr, 20, 1570);
	else if(targetplayer){
		s_sprintfd(targetplayername)("\f%d%s \f4[\f%s\f4]", p==targetplayer?1:isteam(p, targetplayer)?0:3, colorname(targetplayer),
			targetplayerzone==2?"3HEAD":targetplayerzone==1?"2TORSO":"0LEGS");
		draw_text(targetplayername, 20, 1570);
	}
	else draw_textf("\f0Distance\f2: \f1%.1f\f3m", 20, 1570, p->o.dist(worldpos) / 4.f);

	glLoadIdentity();
	glOrtho(0, VIRTW*2, VIRTH*2, 0, -1, 1);

	if(!hideconsole) renderconsole();
	if(!hidestats)
	{
		const int left = (VIRTW-225-10)*2, top = (VIRTH*7/8)*2;
		draw_textf("x %05.3f", left, top-240, p->o.x);
		draw_textf("y %05.3f", left, top-160, p->o.y);
		draw_textf("z %04.1f", left, top-80, p->o.z);
		draw_textf("fps %d", left, top, curfps);
		draw_textf("lod %d", left, top+80, lod_factor());
		draw_textf("wqd %d", left, top+160, nquads);
		draw_textf("wvt %d", left, top+240, curvert);
		draw_textf("evt %d", left, top+320, xtraverts);
	}

	if(hidevote < 2)
	{
		extern votedisplayinfo *curvote;

		if(curvote && curvote->millis >= totalmillis && !(hidevote == 1 && player1->vote != VOTE_YES && curvote->result == VOTE_NEUTRAL))
		{
			int left = 20*2, top = VIRTH;
			if(curvote->result == VOTE_NEUTRAL)
			draw_textf("%s called a vote: %.2f seconds remaining", left, top+240, curvote->owner ? colorname(curvote->owner) : "(unknown owner)", (curvote->expiremillis-lastmillis)/1000.0f);
			else draw_textf("%s called a vote:", left, top+240, curvote->owner ? colorname(curvote->owner) : "(unknown)");
			draw_textf("%s", left, top+320, curvote->desc);
			draw_textf("----", left, top+400);

			vector<playerent *> votepl[VOTE_NUM];
			string votestr[VOTE_NUM];
			if(!watchingdemo) votepl[player1->vote].add(player1);
			loopv(players){
				playerent *vpl = players[i];
				if(!vpl) continue;
				votepl[vpl->vote].add(vpl);
			}
			#define VSU votepl[VOTE_NEUTRAL].length()
			loopl(VOTE_NUM){
				if(l == VOTE_NEUTRAL && VSU > 5) continue;
				votepl[l].sort(votersort);
				s_strcpy(votestr[l], "");
				loopv(votepl[l]){
					playerent *vpl = votepl[l][i];
					if(!vpl) continue;
					s_sprintf(votestr[l])("%s\f%d%s \f6(%d)", votestr[l],
						vpl->priv ? 0 : vpl == player1 ? 6 : vpl->team ? 1 : 3, vpl->name, vpl->clientnum);
					if(vpl->priv) s_sprintf(votestr[l])("%s \f8(x2)", votestr[l]);
					s_strcat(votestr[l], "\f5, ");
				}
				if(!votepl[l].length())
					s_strcpy(votestr[l], "\f4None");
				else
					s_strncpy(votestr[l], votestr[l], strlen(votestr[l])-1);
			}
			#define VSY votepl[VOTE_YES].length()
			#define VSN votepl[VOTE_NO].length()
			draw_textf("%d yes vs. %d no; %d neutral", left, top+480, VSY, VSN, VSU);
			#undef VSY
			#undef VSN

			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
			switch(curvote->result)
			{
				case VOTE_NEUTRAL:
					drawvoteicon(left, top, 0, 0, true);
					if(player1->vote == VOTE_NEUTRAL)
						draw_textf("\f3please vote yes or no", left, top+560);
					else draw_textf("\f2you voted \f%s", left, top+560, player1->vote == VOTE_NO ? "3no" : "0yes");
					break;
				default:
					drawvoteicon(left, top, (curvote->result-1)&1, 1, false);
					draw_textf("\f%s \f%s", left, top+560, veto ? "1VETO" : "2vote", curvote->result == VOTE_YES ? "0PASSED" : "3FAILED");
					break;
			}
			glLoadIdentity();
			glOrtho(0, VIRTW*2.2, VIRTH*2.2, 0, -1, 1);
			left = 44;
			top = (VIRTH+560)*1.1;
			draw_text("\f1Vote \f0Yes", left, top += 88);
			draw_text(votestr[VOTE_YES], left, top += 88);
			draw_text("\f1Vote \f3No", left, top += 88);
			draw_text(votestr[VOTE_NO], left, top += 88);
			if(VSU<=5){
				draw_text("\f1Vote \f2Neutral", left, top += 88);
				draw_text(votestr[VOTE_NEUTRAL], left, top += 88);
			}
			#undef VSU
		}
	}

	if(menu) rendermenu();
	else if(command) renderdoc(40, VIRTH, max(commandh*2 - VIRTH, 0));

	if(!hidehudmsgs) hudmsgs.render();


	if(!hidespecthud && p->state==CS_DEAD && p->spectatemode<=SM_DEATHCAM)
	{
		glLoadIdentity();
		glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
		const int left = (VIRTW*3/2)*6/8, top = (VIRTH*3/2)*3/4;
		draw_textf("SPACE to change view", left, top);
		draw_textf("SCROLL to change player", left, top+80);
	}

	/*
	glLoadIdentity();
	glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
	const int left = (VIRTW*3/2)*4/8, top = (VIRTH*3/2)*3/4;
	draw_textf("!TEST BUILD!", left, top);
	*/

	if(!hidespecthud && spectating && player1->spectatemode!=SM_DEATHCAM)
	{
		glLoadIdentity();
		glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
		draw_text("SPECTATING", VIRTW/40, VIRTH/10*7);
		if(player1->spectatemode==SM_FOLLOW1ST || player1->spectatemode==SM_FOLLOW3RD || player1->spectatemode==SM_FOLLOW3RD_TRANSPARENT)
		{
			if(players.inrange(player1->followplayercn) && players[player1->followplayercn])
			{
				s_sprintfd(name)("Player %s", players[player1->followplayercn]->name);
				draw_text(name, VIRTW/40, VIRTH/10*8);
			}
		}
	}

	if(p->state==CS_ALIVE)
	{
		glLoadIdentity();
		glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);

		if(!hidehudequipment)
		{
			pushfont("huddigits");
			draw_textf("%d",  90, 823, p->health);
			if(p->armour) draw_textf("%d", 360, 823, p->armour);
			if(p->weapons[GUN_GRENADE] && p->weapons[GUN_GRENADE]->mag) p->weapons[GUN_GRENADE]->renderstats();
			// The next set will alter the matrix - load the identity matrix and apply ortho after
			if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS){
				if(p->weaponsel->type != GUN_GRENADE) p->weaponsel->renderstats();
				else if(p->prevweaponsel->type != GUN_GRENADE) p->prevweaponsel->renderstats();
				else if(p->nextweaponsel->type != GUN_GRENADE) p->nextweaponsel->renderstats();
			}
			popfont();
		}

		if(m_flags && !hidectfhud)
		{
			glLoadIdentity();
			glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
			glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			loopi(2) // flag state
			{
				if(flaginfos[i].state == CTFF_INBASE) glDisable(GL_BLEND); else glEnable(GL_BLEND);
				drawctficon(i*120+VIRTW/4.0f*3.0f, 1650, 120, i, 0, 1/4.0f);
			}

			// big flag-stolen icon
			int ft = 0;
			if((flaginfos[0].state==CTFF_STOLEN && flaginfos[0].actor == p) ||
			   (flaginfos[1].state==CTFF_STOLEN && flaginfos[1].actor == p && ++ft))
			{
				glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
				glEnable(GL_BLEND);
				drawctficon(VIRTW-225-10, VIRTH*5/8, 225, ft, 1, 1/2.0f);
				glDisable(GL_BLEND);
			}
		}
	}

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
}

void loadingscreen(const char *fmt, ...)
{
	static Texture *logo = NULL;
	if(!logo) logo = textureload("packages/misc/startscreen.png", 3);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0, 0, 0, 1);
	glColor3f(1, 1, 1);

	loopi(fmt ? 1 : 2)
	{
		glClear(GL_COLOR_BUFFER_BIT);
		quad(logo->id, (VIRTW-VIRTH)/2, 0, VIRTH, 0, 0, 1);
		if(fmt)
		{
			glEnable(GL_BLEND);
			s_sprintfdlv(str, fmt, fmt);
			int w = text_width(str);
			draw_text(str, w>=VIRTW ? 0 : (VIRTW-w)/2, VIRTH*3/4);
			glDisable(GL_BLEND);
		}
		SDL_GL_SwapBuffers();
	}

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
}

static void bar(float bar, int o, float r, float g, float b)
{
	int side = 2*FONTH;
	float x1 = side, x2 = bar*(VIRTW*1.2f-2*side)+side;
	float y1 = o*FONTH;
	glColor3f(0.3f, 0.3f, 0.3f);
	glBegin(GL_TRIANGLE_STRIP);
	loopk(10)
	{
	   float c = 1.2f*cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + 1.2f*sinf(M_PI/2 + k/9.0f*M_PI);
	   glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
	   glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
	}
	glEnd();

	glColor3f(r, g, b);
	glBegin(GL_TRIANGLE_STRIP);
	loopk(10)
	{
	   float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + sinf(M_PI/2 + k/9.0f*M_PI);
	   glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
	   glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
	}
	glEnd();
}

void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2, const char *text2)   // also used during loading
{
	c2skeepalive();

	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, VIRTW*1.2f, VIRTH*1.2f, 0, -1, 1);

	glLineWidth(3);

	if(text1)
	{
		bar(1, 1, 0.1f, 0.1f, 0.1f);
		if(bar1>0) bar(bar1, 1, 0.2f, 0.2f, 0.2f);
	}

	if(bar2>0)
	{
		bar(1, 3, 0.1f, 0.1f, 0.1f);
		bar(bar2, 3, 0.2f, 0.2f, 0.2f);
	}

	glLineWidth(1);

	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	if(text1) draw_text(text1, 2*FONTH, 1*FONTH + FONTH/2);
	if(bar2>0) draw_text(text2, 2*FONTH, 3*FONTH + FONTH/2);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
	SDL_GL_SwapBuffers();
}

