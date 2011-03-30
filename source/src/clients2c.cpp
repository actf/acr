// client processing of the incoming network stream

#include "pch.h"
#include "cube.h"
#include "bot/bot.h"

VARP(networkdebug, 0, 0, 1);
#define DEBUGCOND (networkdebug==1)

extern bool c2sinit, watchingdemo;
extern string clientpassword;

packetqueue pktlogger;

void neterr(const char *s, int info)
{
	conoutf("\f3illegal network message (%s %d)", s, info);

	// might indicate a client/server communication bug, create error report
	pktlogger.flushtolog("packetlog.txt");
	hudoutf("\f3wrote a network error report to packetlog.txt, please post this file to the bugtracker now!");

	disconnect();
}

VARP(autogetmap, 0, 1, 1);

void changemapserv(char *name, int mode, int download)		// forced map change from the server
{
	gamemode = mode;
	if(m_demo) return;
	bool loaded = load_world(name);
	if(download > 0)
	{
		if(securemapcheck(name, false)) return;
		bool sizematch = maploaded == download || download < 10;
		if(loaded && sizematch) return;
		if(autogetmap)
		{
			if(!loaded) getmap(); // no need to ask
			else showmenu("getmap");
		}
		else
		{
			if(!loaded || download < 10) conoutf("\"getmap\" to download the current map from the server");
			else conoutf("\"getmap\" to download a different version of the current map from the server");
		}
	}
}

// update the position of other clients in the game in our world
// don't care if he's in the scenery or other players,
// just don't overlap with our client

void updatepos(playerent *d)
{
	const float r = player1->radius+d->radius;
	const float dx = player1->o.x-d->o.x;
	const float dy = player1->o.y-d->o.y;
	const float dz = player1->o.z-d->o.z;
	const float rz = player1->aboveeye+d->eyeheight;
	const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
	if(fx<r && fy<r && fz<rz && d->state!=CS_DEAD)
	{
		if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
		else	  d->o.x += dx<0 ? r-fx : -(r-fx);
	}
}

void updatelagtime(playerent *d)
{
	int lagtime = totalmillis-d->lastupdate;
	if(lagtime)
	{
		if(d->state!=CS_SPAWNING && d->lastupdate) d->plag = (d->plag*5+lagtime)/6;
		d->lastupdate = totalmillis;
	}
}

extern void trydisconnect();

VARP(maxrollremote, 0, 1, 1); // bound remote "roll" values by our maxroll?!
VARP(voteid, 0, 1, 1); // display votes

void parsepositions(ucharbuf &p)
{
	int type;
	while(p.remaining()) switch(type = getint(p))
	{
		case N_POS:						// position of another client
		{
			int cn = getint(p);
			vec o, vel;
			float yaw, pitch, roll;
			o.x   = getfloat(p);
			o.y   = getfloat(p);
			o.z   = getfloat(p);
			yaw   = getfloat(p);
			pitch = getfloat(p);
			roll  = getfloat(p);
			vel.x = getfloat(p);
			vel.y = getfloat(p);
			vel.z = getfloat(p);
			int f = getuint(p), seqcolor = (f>>6)&1;
			playerent *d = getclient(cn);
			if(!d || seqcolor!=(d->lifesequence&1)) continue;
			vec oldpos(d->o);
			float oldyaw = d->yaw, oldpitch = d->pitch;
			d->o = o;
			d->yaw = yaw;
			d->pitch = pitch;
			d->roll = roll;
			d->vel = vel;
			d->strafe = (f&3)==3 ? -1 : f&3;
			f >>= 2;
			d->move = (f&3)==3 ? -1 : f&3;
			f >>= 2;
			d->onfloor = f&1;
			f >>= 1;
			d->onladder = f&1;
			f >>= 2;
			updatecrouch(d, f&1);
			updatepos(d);
			updatelagtime(d);
			extern int smoothmove, smoothdist;
			if(d->state==CS_DEAD)
			{
				d->resetinterp();
				d->smoothmillis = 0;
			}
			else if(smoothmove && d->smoothmillis>=0 && oldpos.dist(d->o) < smoothdist)
			{
				d->newpos = d->o;
				d->newpos.z -= d->eyeheight;
				d->newyaw = d->yaw;
				d->newpitch = d->pitch;
				d->o = oldpos;
				d->yaw = oldyaw;
				d->pitch = oldpitch;
				oldpos.z -= d->eyeheight;
				(d->deltapos = oldpos).sub(d->newpos);
				d->deltayaw = oldyaw - d->newyaw;
				if(d->deltayaw > 180) d->deltayaw -= 360;
				else if(d->deltayaw < -180) d->deltayaw += 360;
				d->deltapitch = oldpitch - d->newpitch;
				d->smoothmillis = lastmillis;
			}
			else d->smoothmillis = 0;
			if(d->state==CS_LAGGED || d->state==CS_SPAWNING) d->state = CS_ALIVE;
			// when playing a demo spectate first player we know about
			if(player1->isspectating() && player1->spectatemode==SM_NONE) togglespect();
			extern void clamproll(physent *pl);
			if(maxrollremote) clamproll((physent *) d);
			break;
		}

		default:
			neterr("type in position packet: ", type);
			return;
	}
}

SVARP(authkey, "none");

extern votedisplayinfo *curvote;

#include "sha1.h"

void parsemessages(int cn, playerent *d, ucharbuf &p)
{
	static char text[MAXTRANS];
	int type, joining = 0;
	bool demoplayback = false;

	while(p.remaining())
	{
		type = getint(p);

		#ifdef _DEBUG
		if(type!=N_POS && type!=N_PINGTIME && type!=N_PINGPONG && type!=N_CLIENT)
		{
			DEBUGVAR(d);
			ASSERT(type>=0 && type<N_NUM);
			DEBUGVAR(messagenames[type]);
			protocoldebug(true);
		}
		else protocoldebug(false);
		#endif

		switch(type)
		{
			case N_SERVINFO:					// welcome messsage from the server
			{
				int mycn = getint(p), prot = getint(p);
				if(prot != PROTOCOL_VERSION)
				{
					conoutf("\f3you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
					disconnect();
					return;
				}
				sessionid = getint(p);
				player1->clientnum = mycn;
				sendintro();
				break;
			}

			case N_WELCOME:
				joining = getint(p);
				getstring(text, p);
				if(text[0]){
					conoutf("MOTD:");
					conoutf("\f4%s", text);
				}
				player1->resetspec();
				resetcamera();
				break;

			case N_CLIENT:
			{
				int cn = getint(p), len = getuint(p);
				ucharbuf q = p.subbuf(len);
				parsemessages(cn, getclient(cn), q);
				break;
			}

			case N_SOUND:
			{
				int cn = getint(p);
				playerent *d = getclient(cn);
				if(!d) d = player1;
				playsound(getint(p), d);
				break;
			}

			case N_TEXT:
			{
				int cn = getint(p), voice = getint(p), flags = (voice >> 5) & 7;
				voice = (voice & 0x1F) + S_MAINEND;
				getstring(text, p);
				filtertext(text, text);
				playerent *d = getclient(cn);
				if(d) saytext(d, text, flags, voice);
				else if(cn == -1) hudoutf("\f3Server Admin: \f5%s", text);
				break;
			}

			case N_MAPCHANGE:
			{
				// get map info
				int mode = getint(p), downloadable = getint(p);
				getstring(text, p);
				changemapserv(text, mode, downloadable);
				// get items
				int n;
				resetspawns();
				while((n = getint(p)) != -1 && !p.overread()) setspawn(n, true);
				break;
			}

			case N_NEXTMAP: // server requests next map
			{
				getint(p);
				s_sprintfd(nextmapalias)("nextmap_%s", getclientmap());
				const char *map = getalias(nextmapalias);	 // look up map in the cycle
				changemap(map ? map : getclientmap());
				break;
			}

			case N_SWITCHTEAM:
			{
				int t = getint(p);
                if(m_team) conoutf("\f3Team %s is full", team_string(t));
				break;
			}

			case N_INITCLIENT:
			{
				playerent *d = newclient(getint(p));
				d->team = getint(p);
				setskin(d, getint(p));
				getstring(text, p);
				filtername(d->name, text);
				if(!*text) s_strcpy(text, "unnamed");
				s_strcpy(d->name, text);
				conoutf("connected: %s", colorname(d));
				if(!joining){
					s_sprintfd(joinmsg)("%s \f0joined \f2the \f1game", colorname(d));
					chatout(joinmsg);
				}
				updateclientname(d);
				if(m_flags) loopi(2)
				{
					flaginfo &f = flaginfos[i];
					if(!f.actor) f.actor = getclient(f.actor_cn);
				}
				break;
			}

			case N_NEWNAME:
			{
				playerent *d = getclient(getint(p));
				getstring(text, p);
				filtertext(text, text, 1, MAXNAMELEN);
				if(!text[0]) s_strcpy(text, "unnamed");
				if(!d || !strcmp(d->name, text)) break;
				if(d->name[0]) conoutf("%s \f6(%d) \f5is now known as \f1%s", d->name, d->clientnum, text);
				filtername(d->name, text);
				if(d == player1) alias("curname", player1->name);
				updateclientname(d);
				break;
			}

			case N_SKIN:
			{
				playerent *d = getclient(getint(p));
				int s = getint(p);
				if(d) setskin(d, s);
				break;
			}

			case N_CDIS:
			{
				int cn = getint(p), reason = getint(p);
				playerent *d = getclient(cn);
				if(!d) break;
				if(*d->name){
					conoutf("player %s disconnected (%s)", colorname(d), reason >= 0 ? disc_reason(reason) : "normally");
					s_sprintfd(leavemsg)("%s \f3left \f2the \f1game", colorname(d));
					chatout(leavemsg);
				}
				zapplayer(players[cn]);
				break;
			}

			case N_EDITMODE:
			{
				int cn = getint(p), val = getint(p);
				playerent *d = getclient(cn);
				if(d) d->state = val ? CS_EDITING : CS_ALIVE;
				break;
			}

			case N_SPAWN:
			{
				playerent *s = d;
				if(!s) { static playerent dummy; s = &dummy; }
				s->respawn();
				s->lifesequence = getint(p);
				s->health = getint(p);
				s->armour = getint(p);
				int gunselect = getint(p);
				s->setprimary(gunselect);
				s->selectweapon(gunselect);
				loopi(NUMGUNS) s->ammo[i] = getint(p);
				loopi(NUMGUNS) s->mag[i] = getint(p);
				s->state = CS_SPAWNING;
				break;
			}

			case N_SPAWNSTATE:
			{
				if(editmode) toggleedit(true);
				showscores(false);
				setscope(false);
				player1->respawn();
				player1->lifesequence = getint(p);
				player1->health = getint(p);
				player1->armour = getint(p);
				player1->setprimary(getint(p));
				player1->selectweapon(getint(p));
				int arenaspawn = getint(p);
				loopi(NUMGUNS) player1->ammo[i] = getint(p);
				loopi(NUMGUNS) player1->mag[i] = getint(p);
				player1->state = CS_ALIVE;
				findplayerstart(player1, false, arenaspawn);
				extern int nextskin;
				if(player1->skin!=nextskin) setskin(player1, nextskin);
				arenaintermission = 0;
				if(m_duel)
				{
					closemenu(NULL);
					conoutf("new round starting... fight!");
					hudeditf(HUDMSG_TIMER, "FIGHT!");
				}
				addmsg(N_SPAWN, "rii", player1->lifesequence, player1->weaponsel->type);
				player1->weaponswitch(player1->primweap);
				player1->weaponchanging -= weapon::weaponchangetime/2;
				break;
			}

			case N_SCOPE:
			{
				int cn = getint(p); playerent *d = getclient(cn);
				bool scope = getint(p) != 0;
				if(!d || d->state != CS_ALIVE || !ads_gun(d->weaponsel->type)) break;
				d->scoping = scope;
				break;
			}

			case N_SG: // someone else's shotgun rays
			{	
				extern vec sg[SGRAYS];
				loopi(SGRAYS) loopj(3) sg[i][j] = getfloat(p);
				break;
			}

			case N_SHOOT:
			case N_SHOOTC:
			{
				int scn = getint(p), gun = getint(p);
				vec from, to;
				if(type == N_SHOOTC) from = to = vec(0, 0, 0);
				else{
					loopk(3) from[k] = getfloat(p);
					loopk(3) to[k] = getfloat(p);
				}
				playerent *s = getclient(scn);
				if(!s || !weapon::valid(gun) || s == player1) break;
				s->mag[gun]--;
				updatelastaction(s);
				if(s->weapons[gun]){
					s->weapons[gun]->gunwait = s->weapons[gun]->info.attackdelay;
					s->lastattackweapon = s->weapons[gun];
					s->weapons[gun]->attackfx(from, to, -1);
				}
				break;
			}

			case N_THROWNADE:
			{
				vec from, to;
				loopk(3) from[k] = getfloat(p);
				loopk(3) to[k] = getfloat(p);
				int nademillis = getint(p);
				if(!d) break;
				updatelastaction(d);
				d->lastattackweapon = d->weapons[GUN_GRENADE];
				if(d->weapons[GUN_GRENADE]) d->weapons[GUN_GRENADE]->attackfx(from, to, nademillis);
				break;
			}

			case N_RELOAD:
			{
				int cn = getint(p), gun = getint(p), mag = getint(p), ammo = getint(p);
				playerent *p = getclient(cn);
				if(!p || gun < 0 || gun >= NUMGUNS) break;
				if(p != player1) p->weapons[gun]->reload();
				p->ammo[gun] = ammo;
				p->mag[gun] = mag;
				break;
			}

			case N_POINTS:
			{
				int cn = getint(p), points = getint(p);
				playerent *d = getclient(cn);
				if(d) d->points = points;
				break;
			}

			case N_REGEN:
			{
				int cn = getint(p), amt = getint(p);
				playerent *d = getclient(cn);
				if(!d) break;
				d->health += amt;
				d->lastregen = lastmillis;
				if(d == player1){
					playsound(S_ITEMHEALTH, SP_LOW);
				}
				break;
			}

			case N_DAMAGE:
			{
				int tcn = getint(p),
					acn = getint(p),
					damage = getint(p),
					armour = getint(p),
					health = getint(p),
					weap = getint(p);
				playerent *target = getclient(tcn), *actor = getclient(acn);
				if(!target) break;
				target->armour = armour;
				target->health = health;
				if(!actor) break;
				dodamage(damage, target, actor, weap);
				break;
			}

			case N_PROJPUSH:
			{
				int cn = getint(p), gun = getint(p), damage = getint(p);
				vec src; loopk(3) src[k] = getfloat(p);
				playerent *d = getclient(cn);
				if(gun != GUN_GRENADE) break; // only type of projectile right now!
				d->damagestack.add(damageinfo(src, lastmillis));
				if(d != player1 || d->o == src) break;
				vec dir = d->o;
				dir.sub(src).normalize();
				d->hitpush(damage, dir, gun);
				break;
			}

			case N_KILL:
			{
				int vcn = getint(p), acn = getint(p), frags = getint(p), weap = getint(p), style = getint(p) & FRAG_SERVER, damage = getint(p), assists = getint(p) & 0xFF;
				float killdist = getfloat(p);
				playerent *victim = getclient(vcn), *actor = getclient(acn);
				if(actor) actor->frags = frags;
				if(victim){
					victim->damagelog.setsize(0);
					loopi(assists) victim->damagelog.add(getint(p));
				} else loopi(assists) getint(p);
				if(!actor || !victim) break;
				if((victim->health -= damage) > 0) victim->health = 0;
				dodamage(damage, victim, actor, weap);
				dokill(victim, actor, weap, damage, style, killdist);
				break;
			}

			case N_RESUME:
			{
				loopi(MAXCLIENTS)
				{
					int cn = getint(p);
					if(p.overread() || cn<0) break;
					int state = getint(p), lifesequence = getint(p), gunselect = getint(p), points = getint(p), flagscore = getint(p), frags = getint(p), assists = getint(p), killstreak = getint(p), deaths = getint(p), health = getint(p), armour = getint(p);
					int ammo[NUMGUNS], mag[NUMGUNS];
					loopi(NUMGUNS) ammo[i] = getint(p);
					loopi(NUMGUNS) mag[i] = getint(p);
					playerent *d = newclient(cn);
					if(!d) continue;
					if(d!=player1) d->state = state;
					d->lifesequence = lifesequence;
					d->points = points;
					d->flagscore = flagscore;
					d->frags = frags;
					d->assists = assists;
					d->killstreak = killstreak;
					d->deaths = deaths;
					if(d!=player1)
					{
						int primary = GUN_KNIFE;
						if(m_osok) primary = GUN_SNIPER;
						else if(m_pistol) primary = GUN_PISTOL;
						else if(!m_lss)
						{
							if(gunselect < GUN_GRENADE) primary = gunselect;
							loopi(GUN_GRENADE) if(ammo[i] || mag[i]) primary = max(primary, i);
							if(primary <= GUN_PISTOL) primary = GUN_ASSAULT;
						}
						d->setprimary(primary);
						d->selectweapon(gunselect);
						d->health = health;
						d->armour = armour;
						memcpy(d->ammo, ammo, sizeof(ammo));
						memcpy(d->mag, mag, sizeof(mag));
					}
				}
				break;
			}

			case N_ITEMSPAWN:
			{
				int i = getint(p);
				setspawn(i, true);
				break;
			}

			case N_ITEMACC:
			{
				int i = getint(p), cn = getint(p);
				playerent *d = cn==getclientnum() ? player1 : getclient(cn);
				pickupeffects(i, d);
				break;
			}

			case N_EDITH:			  // coop editing messages, should be extended to include all possible editing ops
			case N_EDITT:
			case N_EDITS:
			case N_EDITD:
			case N_EDITE:
			{
				int x  = getint(p);
				int y  = getint(p);
				int xs = getint(p);
				int ys = getint(p);
				int v  = getint(p);
				block b = { x, y, xs, ys };
				switch(type)
				{
					case N_EDITH: editheightxy(v!=0, getint(p), b); break;
					case N_EDITT: edittexxy(v, getint(p), b); break;
					case N_EDITS: edittypexy(v, b); break;
					case N_EDITD: setvdeltaxy(v, b); break;
					case N_EDITE: editequalisexy(v!=0, b); break;
				}
				break;
			}

			case N_NEWMAP:
			{
				int size = getint(p);
				if(size>=0) empty_world(size, true);
				else empty_world(-1, true);
				if(d && d!=player1)
					conoutf(size>=0 ? "%s started a new map of size %d" : "%s enlarged the map to size %d", colorname(d), sfactor);
				break;
			}

			case N_EDITENT:			// coop edit of ent
			{
				uint i = getint(p);
				while((uint)ents.length()<=i) ents.add().type = NOTUSED;
				int to = ents[i].type;
				ents[i].type = getint(p);
				ents[i].x = getint(p);
				ents[i].y = getint(p);
				ents[i].z = getint(p);
				ents[i].attr1 = getint(p);
				ents[i].attr2 = getint(p);
				ents[i].attr3 = getint(p);
				ents[i].attr4 = getint(p);
				ents[i].spawned = false;
				if(ents[i].type==LIGHT || to==LIGHT) calclight();
				if(ents[i].type==SOUND) preloadmapsound(ents[i]);
				break;
			}

			case N_PINGPONG:
				addmsg(N_PINGTIME, "i", totalmillis - getint(p));
				break;

			case N_PINGTIME:
			{
				playerent *pl = getclient(getint(p));
				int pp = getint(p);
				if(pl) pl->ping = pp;
				break;
			}

			case N_MAPIDENT:
				conoutf("\f3please \f1get the map \f3by typing \f0/getmap");
				break;

			case N_SENDMAP:
			{
				int cn = getint(p); playerent *d = getclient(cn);
				getstring(text, p);
				conoutf("%s \f0sent the map %s to the server, \f1type /getmap to get it", d ? colorname(d) : "someone", text);
				break;
			}

			case N_FF:
				hudoutf("\f3FRIENDLY FIRE WILL NOT BE TOLERATED!\n\f2Wait %d seconds until to respawn!", getint(p));
				break;

			case N_CONFMSG: // preconfigured messages
			{
				int n = getint(p);
				string msg = "server sent unknown message";
				switch(n){
					case 10: // 1* maps
						s_strcpy(msg, "\f3coopedit is restricted to admins!");
						break;
					case 11:
						s_strcpy(msg, "\f3map not found - start another map or send the map to the server");
						break;
					case 12:
						s_strcpy(msg, "\f3the server does not have this map");
						break;
					case 13:
						s_strcpy(msg, "no map to get");
						break;
					case 14:
					{
						int minutes = getint(p), mode = getint(p), nextt = (mode >> 6) & 3; mode &= 0x3F;
						getstring(text, p);
						s_strcpy(msg, "nextmap:");
						switch(nextt){
							case 1:
								s_strcpy(msg, "\f2Next map loaded:");
								break;
							case 2:
								s_strcpy(msg, "\f1Next on map rotation:");
								break;
							case 3:
								s_strcpy(msg, "\f2No map rotation entries,");
								break;
						}
						s_sprintf(msg)("%s %s in mode %s for %d minutes", msg, text, modestr(mode), minutes);
						break;
					}
					case 15:
					{
						getstring(text, p);
						bool spawns = (*text & 0x80) != 0, flags = (*text & 0x40) != 0;
						*text &= 0x3F;
						s_sprintf(msg)("\f3map \"%s\" does not support \"%s\": ", text + 1, modestr(*text));
						if(spawns || !flags) s_strcat(msg, "player spawns");
						if(spawns && flags) s_strcat(msg, " and ");
						if(flags || !spawns) s_strcat(msg, "flag bases");
						s_strcat(msg, " missing");
					}
					case 20: // 2* demos
						s_strcpy(msg, "recording demo");
						break;
					case 21:
						s_strcpy(msg, "demo playback finished");
						break;
					case 22:
					case 23:
					case 24:
					case 25:
					case 26:
						getstring(text, p);
						switch(n){
							case 22:
								s_sprintf(msg)("could not read demo \"%s\"", text);
								break;
							case 23:
								s_sprintf(msg)("\"%s\" is not a demo file", text);
								break;
							case 24:
							case 25:
								s_sprintf(msg)("demo \"%s\" requires a %s version", text, n==24 ? "older" : "newer");
								break;
							case 26:
								s_sprintf(msg)("Demo \"%s\" recorded", text);
								break;
						}
						break;
					case 27:
						s_strcpy(msg, "no demos available");
						break;
					case 28:
						s_sprintf(msg)("no demo %d available", getint(p)); 
						break;
					case 29:
						s_sprintf(msg)("you need %s to download demos", privname(getint(p)));
						break;
					case 40: // 4* gameplay errors
					{
						int cn = getint(p); playerent *d = getclient(cn);
						s_strcpy(msg, "unknown");
						if(d) s_strcpy(msg, colorname(d));
						s_strcat(msg, " ");
						switch(n){
							case 40:
								s_strcat(msg, "\f2collides with the map \f5- \f3forcing death");
								break;
						}
						break;
					}
				}
				conoutf("%s", msg);
				break;
			}

			case N_ACCURACY:
			{
				int hit = getint(p), shot = getint(p);
				s_sprintfd(accmsg)("\f1%d%% \f5accuracy \f4(\f1%d \f2shot, \f1%d \f0hit, \f1%d \f3wasted\f4)",
					hit * 100 / max(shot, 1), hit, shot, shot - hit);
				break;
			}

			case N_TIMEUP:
				timeupdate(getint(p));
				break;

			case N_QUICKSWITCH:
			{
				int cn = getint(p);
				playerent *d = getclient(cn);
				if(!d) break;
				d->weaponchanging = lastmillis-1-(weapon::weaponchangetime/2);
				d->nextweaponsel = d->weaponsel = d->primweap;
				break;
			}

			case N_SWITCHWEAP:
			{
				int cn = getint(p), gun = getint(p);
				playerent *d = getclient(cn);
				if(!d || gun < 0 || gun >= NUMGUNS) break;
				d->ads = 0;
				d->weaponswitch(d->weapons[gun]);
				//if(!d->weaponchanging) d->selectweapon(gun);
				break;
			}

			case N_SERVMSG:
				getstring(text, p);
				conoutf("%s", text);
				break;

			case N_FLAGINFO:
			{
				int flag = getint(p);
				if(flag<0 || flag>1) return;
				flaginfo &f = flaginfos[flag];
				f.state = getint(p);
				switch(f.state)
				{
					case CTFF_STOLEN:
						flagstolen(flag, getint(p));
						break;
					case CTFF_DROPPED:
					{
						float x = getfloat(p), y = getfloat(p), z = getfloat(p);
						flagdropped(flag, x, y, z);
						break;
					}
					case CTFF_INBASE:
						flaginbase(flag);
						break;
					case CTFF_IDLE:
						flagidle(flag);
						break;
				}
				break;
			}

			case N_FLAGMSG:
			{
				int flag = getint(p);
				int message = getint(p);
				int actor = getint(p);
				int flagtime = message == FA_KTFSCORE ? getint(p) : -1;
				flagmsg(flag, message, actor, flagtime);
				break;
			}

			case N_FLAGCNT:
			{
				int fcn = getint(p);
				int flags = getint(p);
				playerent *p = (fcn == getclientnum() ? player1 : getclient(fcn));
				if(p) p->flagscore = flags;
				break;
			}

			case N_ARENAWIN:
			{
				int acn = getint(p); playerent *alive = getclient(acn);
				conoutf("the round is over! next round in 5 seconds...");
				if(!alive) hudoutf("everyone died!");
				else if(m_team) hudoutf(alive==player1 ? "you are the victor for your team!" : "team %s is the victor!", team_string(alive->team));
				else if(alive==player1) hudoutf("you are the victor!");
				else hudoutf("%s is the victor!", colorname(alive));
				arenaintermission = lastmillis;
				break;
			}

			case N_FORCEGIB:
			case N_FORCEDEATH:
			{
				int cn = getint(p);
				playerent *d = newclient(cn);
				if(!d) break;
				if(type == N_FORCEGIB) addgib(d);
				deathstate(d);
				break;
			}

			case N_SETROLE:
			{
				int cl = getint(p), r = getint(p);
				playerent *p = newclient(cl);
				if(!p) break;
				p->priv = r;
				break;
			}

			case N_ROLECHANGE:
			{
				int cl = getint(p), r = getint(p);
				bool drop = (r >> 7) & 1, err = (r >> 6) & 1; r &= 0x3F;
				playerent *d = getclient(cl);
				const char *n = (d == player1) ? "\f1you" : colorname(d);
				if(!d) break;
				if(err){
					if(d == player1) hudoutf("\f1you \f3already \f2have \f%d%s \f5status", privcolor(r), privname(r));
					else hudoutf("\f3there is already a \f1master \f2(\f0%s\f2)", n);
					break;
				}
				(d == player1 ? hudoutf : conoutf)("%s \f2%s \f%d%s \f5status", n, drop ? "relinquished" : "claimed", privcolor(r), privname(r));
				break;
			}

			case N_AUTHREQ:
			{
				int nonce = getint(p);
				if(nonce < 0){
					conoutf("server challenged incorrectly");
					break;
				}
				conoutf("server is challenging authentication details");
				string buf;
				itoa(buf, nonce);
				s_strcat(buf, authkey);
				unsigned message_digest[5];
				SHA1 sha;
				sha << buf;
				if(!sha.Result(message_digest)){
					conoutf("could not compute message digest");
					break;
				}
				s_sprintfd(answer)("%x%x%x%x%x", message_digest[0], message_digest[1], message_digest[2], message_digest[3], message_digest[4]);
				s_strncpy(answer, answer, 41); // 40 hex digits
				addmsg(N_AUTHCHAL, "rs", answer);
				break;
			}

			case N_AUTHCHAL:
			{
				switch(getint(p)){
					case 0:
						conoutf("please wait, requesting challenge");
						break;
					case 1:
						conoutf("waiting for previous attempt...");
						break;
					case 2:
						conoutf("not connected to authentication server");
						break;
					case 3:
						conoutf("authority request failed");
						break;
					case 4:
						conoutf("please wait, requesting credential match");
						break;
					case 5:
					{
						int cn = getint(p); getstring(text, p);
						playerent *d = getclient(cn);
						if(!d) break;
						filtertext(text, text, 1, MAXNAMELEN);
						(d == player1 ? hudoutf : conoutf)("%s \f1identified as \f2'\f9%s\f2'", colorname(d), text);
						break;
					}
				}
				break;
			}

			case N_SETTEAM:
			{
				int cn = getint(p), fnt = getint(p), ftr = fnt >> 4; fnt &= 0xF;
				playerent *p = getclient(cn);
				if(!p) break;
				string nts;
				s_strcpy(nts, fnt ? "\f1" : "\f3");
				s_strcat(nts, team_string(fnt));
				if(p->team == fnt){
					if(p == player1 && ftr == FTR_AUTOTEAM) hudoutf("\f2you stay in team %s", nts);
					break;
				}
				p->team = fnt;
				if(p == player1 && !watchingdemo){
					switch(ftr){
						case FTR_PLAYERWISH:
							conoutf("\f2you're now in team %s", nts);
							break;
						case FTR_AUTOTEAM:
							hudoutf("\f2the server forced you to team %s", nts);
							break;
					}
				}
				else{
                    switch(ftr)
                    {
                        case FTR_PLAYERWISH:
                            conoutf("\f2%s switched to team %s", colorname(p), nts);
                            break;
                        case FTR_AUTOTEAM:
                            conoutf("\f2the server forced %s to team %s", colorname(p), nts);
                            break;
                    }
                }
				break;
			}

			case N_CALLVOTE:
			{
				int cn = getint(p), type = getint(p), votewasted = getint(p);
				playerent *d = getclient(cn);
				if(type < 0 || type >= SA_NUM || !d) return;
				votedisplayinfo *v = NULL;
				string a;
				switch(type)
				{
					case SA_MAP:
						getstring(text, p);
						filtertext(text, text);
						itoa(a, getint(p));
						v = newvotedisplayinfo(d, type, text, a);
						break;
					case SA_SERVERDESC:
						getstring(text, p);
						filtertext(text, text);
						v = newvotedisplayinfo(d, type, text, NULL);
						break;
					case SA_STOPDEMO:
					case SA_REMBANS:
					case SA_SHUFFLETEAMS:
						v = newvotedisplayinfo(d, type, NULL, NULL);
						break;
					case SA_BAN:
					case SA_GIVEADMIN:
						itoa(a, getint(p));
						itoa(text, getint(p));
						v = newvotedisplayinfo(d, type, a, text);
						break;
					default:
						itoa(a, getint(p));
						v = newvotedisplayinfo(d, type, a, NULL);
						break;
				}
				if(v) v->expiremillis = totalmillis;
				if(type == SA_KICK) v->expiremillis += 35000;
				else if(type == SA_BAN) v->expiremillis += 25000;
				else v->expiremillis += 40000;
				v->expiremillis -= votewasted;
				displayvote(v);
				break;
			}

			case N_CALLVOTEERR:
			{
				int e = getint(p);
				if(e < 0 || e >= VOTEE_NUM) break;
				conoutf("\f3could not vote: %s", voteerrorstr(e));
				break;
			}

			case N_VOTE:
			{
				int cn = getint(p), vote = getint(p);
				playerent *d = getclient(cn);
				if(!curvote || !d || vote < VOTE_NEUTRAL || vote > VOTE_NO) break;
				d->vote = vote;
				if(vote == VOTE_NEUTRAL) break;
				d->voternum = curvote->nextvote++;
				if(voteid && (d != curvote->owner || curvote->millis + 100 < lastmillis))
					conoutf("%s \f6(%d) \f2voted \f%s", d->name, cn, vote == VOTE_NO ? "3no" : "0yes");
				break;
			}

			case N_VOTERESULT:
			{
				int v = getint(p);
				veto = ((v >> 7) & 1) > 0;
				v &= 0x7F;
				if(curvote && v >= 0 && v < VOTE_NUM)
				{
					curvote->result = v;
					curvote->millis = totalmillis + 5000;
					conoutf("vote %s", v == VOTE_YES ? "\f0passed" : "\f3failed");
					playsound(v == VOTE_YES ? S_VOTEPASS : S_VOTEFAIL, SP_HIGH);
				}
				break;
			}

			case N_WHOIS:
			{
				int cn = getint(p), wants = getint(p), ip = getint(p), mask = getint(p), realmask = (mask >> 6) & 0x3F; mask &= 0x3F;
				playerent *pl = getclient(cn), *owner = getclient(wants);
				s_sprintfd(cip)("%d", ip & 0xFF);
				if(mask > 8 || (ip >> 8) & 0xFF){
					s_sprintf(cip)("%s.%d", cip, (ip >> 8) & 0xFF);
					if(mask > 16 || (ip >> 16) & 0xFF){
						s_sprintf(cip)("%s.%d", cip, (ip >> 16) & 0xFF);
						if(mask > 24 || (ip >> 24) & 0xFF) s_sprintf(cip)("%s.%d", cip, (ip >> 24) & 0xFF);
					}
				}
				if(mask < 32) s_sprintf(cip)("%s\f7/\f4%d", cip, mask);
				if(mask != realmask) s_sprintf(cip)("%s|d", cip, realmask);
				conoutf("\fs%s \f1requests \f2who\f0is \f3 on \fr%s \f6(\f5%s\f6)", owner ? colorname(owner) : "someone", pl ? colorname(pl) : "unknown", cip);
				break;
			}

			case N_LISTDEMOS:
			{
				int demos = getint(p);
				if(!demos) conoutf("no demos available");
				else loopi(demos)
				{
					getstring(text, p);
					conoutf("%d. %s", i+1, text);
				}
				break;
			}

			case N_DEMOPLAYBACK:
			{
				watchingdemo = demoplayback = getint(p)!=0;
				if(demoplayback)
				{
					getstring(text, p);
					conoutf("playing demo \"%s\"", text);
					player1->resetspec();
					player1->state = CS_SPECTATE;
				}
				else
				{
					// cleanups
					loopv(players) zapplayer(players[i]);
					clearvote();
					player1->state = CS_ALIVE;
					player1->resetspec();
				}
				player1->clientnum = getint(p);
				break;
			}

			case N_EXT:
			{
				getstring(text, p, 64);
				int len = getint(p);
				if(len > 50) { neterr("Extension too long,", len); return; }
				//if(!strcmp(text, ""));
				else{ // ignore unknown extensions
					conoutf("server sent unknown extension %s, length %d", text, len);
					while(len-- > 0) getint(p);
				}
				break;
			}

			default:
				if(cn < 0) neterr("type in game packet", type);
				else conoutf("\f3illegal network message type (%d)", type);
				return;
		}
	}

	// check if joining here so as not to interrupt welcomepacket
	if(joining<0 && *getclientmap()){ // we are the first client on this server, set map
		nextmode = gamemode;
		changemap(getclientmap());
	}

	#ifdef _DEBUG
	protocoldebug(false);
	#endif
}

void receivefile(uchar *data, int len)
{
	ucharbuf p(data, len);
	int type = getint(p);
	data += p.length();
	len -= p.length();
	switch(type)
	{
		case N_DEMO:
		{
			s_sprintfd(fname)("demos/%s.dmo", timestring());
			path(fname);
			FILE *demo = openfile(fname, "wb");
			if(!demo)
			{
				conoutf("failed writing to \"%s\"", fname);
				return;
			}
			conoutf("received demo \"%s\"", fname);
			fwrite(data, 1, len, demo);
			fclose(demo);
			break;
		}

		case N_RECVMAP:
		{
			static char text[MAXTRANS];
			getstring(text, p);
			conoutf("received map \"%s\" from server, reloading..", text);
			int mapsize = getint(p);
			int cfgsize = getint(p);
			int cfgsizegz = getint(p);
			int size = mapsize + cfgsizegz;
			if(p.remaining() < size)
			{
				p.forceoverread();
				break;
			}
			if(securemapcheck(text))
			{
				p.len += size;
				break;
			}
			writemap(path(text), mapsize, &p.buf[p.len]);
			p.len += mapsize;
			writecfggz(path(text), cfgsize, cfgsizegz, &p.buf[p.len]);
			p.len += cfgsizegz;
			break;
		}

		default:
			p.len = 0;
			parsemessages(-1, NULL, p);
			break;
	}
}

void servertoclient(int chan, uchar *buf, int len)   // processes any updates from the server
{
	ucharbuf p(buf, len);
	switch(chan)
	{
		case 0: parsepositions(p); break;
		case 1: parsemessages(-1, NULL, p); break;
		case 2: receivefile(p.buf, p.maxlen); break;
	}
}

void localservertoclient(int chan, uchar *buf, int len)   // processes any updates from the server
{
	servertoclient(chan, buf, len);
}
