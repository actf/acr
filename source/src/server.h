// server.h

#define gamemode smode   // allows the gamemode macros to work with the server mode

#define valid_flag(f) (f >= 0 && f < 2)

enum { GE_NONE = 0, GE_SHOT, GE_PROJ, GE_AKIMBO, GE_RELOAD };
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP, ST_AI };

static int interm = 0, minremain = 0, gamemillis = 0, gamelimit = 0;
static const int DEATHMILLIS = 300;
int smode = GMODE_TEAMDEATHMATCH, mastermode = MM_OPEN, botbalance = -1;

#define eventcommon int type, millis, id
struct shotevent{
	eventcommon;
	int gun;
	float to[3];
	bool compact;
};

struct projevent{
	eventcommon;
	int gun, flag;
	float o[3];
};

struct akimboevent{
	eventcommon;
};

struct reloadevent{
	eventcommon;
	int gun;
};

union gameevent{
	struct { eventcommon; };
	shotevent shot;
	projevent proj;
	akimboevent akimbo;
	reloadevent reload;
};


template <int N>
struct projectilestate
{
	int projs[N];
	int numprojs;
	int throwable;

	projectilestate() : numprojs(0) {}

	void reset() { numprojs = 0; }

	void add(int val)
	{
		if(numprojs>=N) numprojs = 0;
		projs[numprojs++] = val;
		++throwable;
	}

	bool remove(int val)
	{
		loopi(numprojs) if(projs[i]==val)
		{
			projs[i] = projs[--numprojs];
			return true;
		}
		return false;
	}
};

struct clientstate : playerstate
{
	vec o, aim, vel, lasto, sg[SGRAYS], flagpickupo;
	float pitchvel;
	int state, lastomillis, movemillis;
	int lastdeath, lastffkill, lastspawn, lifesequence, spawnmillis;
	int lastkill, combo;
	bool crouching;
	int crouchmillis, scopemillis;
	int drownmillis; char drownval;
	int streakondeath;
	int lastshot, lastregen;
	projectilestate<6> grenades; // 5000ms TLL / (we can throw one every 650ms+200ms) = 6 nades possible
	projectilestate<3> knives;
	int akimbos, akimbomillis;
	int points, flagscore, frags, deaths, shotdamage, damage;
	ivector revengelog;

	clientstate() : state(CS_DEAD), playerstate() {}

	bool isalive(int gamemillis)
	{
		if(interm) return false;
		return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
	}

	bool waitexpired(int gamemillis)
	{
		int wait = gamemillis - lastshot;
		loopi(WEAP_MAX) if(wait < gunwait[i]) return false;
		return true;
	}

	void reset()
	{
		state = CS_DEAD;
		lifesequence = -1;
		grenades.reset();
		knives.reset();
		akimbos = akimbomillis = 0;
		points = flagscore = frags = deaths = shotdamage = damage = lastffkill = 0;
		radarearned = airstrikes = nukemillis = 0;
		revengelog.setsize(0);
		respawn();
	}

	void respawn()
	{
		playerstate::respawn(); spawnmillis = 0; // move spawnmillis to playerstate for clients to have opacity...
		o = lasto = vec(-1e10f, -1e10f, -1e10f);
		aim = vel = vec(0, 0, 0);
		pitchvel = 0;
		lastomillis = movemillis = 0;
		drownmillis = drownval = 0;
		lastspawn = -1;
		lastdeath = lastshot = lastregen = 0;
		lastkill = combo = 0;
		akimbos = akimbomillis = 0;
		damagelog.setsize(0);
		crouching = false;
		crouchmillis = scopemillis = 0;
		streakondeath = -1;
	}

	int protect(int millis){
		const int delay = SPAWNPROTECT, spawndelay = millis - spawnmillis;
		int amt = 0;
        if(ownernum < 0 && spawnmillis && delay && spawndelay <= delay) amt = delay - spawndelay;
        return amt;
	}
};

struct savedscore
{
	string name;
	uint ip;
	int points, frags, assists, killstreak, flagscore, deaths, shotdamage, damage;

	void save(clientstate &cs)
	{
		points = cs.points;
		frags = cs.frags;
		assists = cs.assists;
		killstreak = cs.killstreak;
		flagscore = cs.flagscore;
		deaths = cs.deaths;
		shotdamage = cs.shotdamage;
		damage = cs.damage;
	}

	void restore(clientstate &cs)
	{
		cs.points = points;
		cs.frags = frags;
		cs.assists = assists;
		cs.killstreak = killstreak;
		cs.flagscore = flagscore;
		cs.deaths = deaths;
		cs.shotdamage = shotdamage;
		cs.damage = damage;
	}
};

struct client				   // server side version of "dynent" type
{
	int type;
	int clientnum;
	ENetPeer *peer;
	string hostname;
	string name;
	int ping, team, skin, vote, priv;
	int connectmillis;
	bool connected, connectauth;
	int authtoken, authmillis, authpriv, masterverdict; uint authreq;
	bool haswelcome;
	bool isonrightmap;
	bool timesync;
	int overflow;
	int gameoffset, lastevent, lastvotecall;
	int demoflags;
	clientstate state;
	vector<gameevent> events, timers;
	vector<uchar> position, messages;
	string lastsaytext;
	int saychars, lastsay, spamcount;
	int at3_score, at3_lastforce, eff_score;
	bool at3_dontmove;
	int spawnindex;
	int salt;
	string pwd;
	int mapcollisions;
	enet_uint32 bottomRTT;

	gameevent &addevent()
	{
		static gameevent dummy;
		if(events.length()>32) return dummy;
		return events.add();
	}

	gameevent &addtimer(){
		static gameevent dummy;
		if(timers.length()>320) return dummy;
		return timers.add();
	}

	void removetimers(int type){ loopv(timers) if(timers[i].type == type) timers.remove(i--); }

	void removeexplosives() { state.grenades.reset(); state.knives.reset(); removetimers(GE_PROJ); }

	void suicide(int weap, int flags = FRAG_NONE, int damage = 2000){
		extern void serverdamage(client *target, client *actor, int damage, int gun, int style, const vec &source);
		if(state.state != CS_DEAD) serverdamage(this, this, damage, weap, flags, state.o);
	}

	void mapchange()
	{
		state.reset();
		events.setsize(0);
		timers.setsize(0);
		overflow = 0;
		timesync = false;
		isonrightmap = m_edit;
		lastevent = 0;
		at3_lastforce = 0;
		mapcollisions = 0;
	}

	void reset()
	{
		name[0] = demoflags = authmillis = 0;
		ping = bottomRTT = 9999;
		skin = team = 0;
		position.setsize(0);
		messages.setsize(0);
		connected = connectauth = haswelcome = false;
		priv = PRIV_NONE;
		lastvotecall = 0;
		vote = VOTE_NEUTRAL;
		lastsaytext[0] = '\0';
		saychars = authreq = 0;
		spawnindex = -1;
		mapchange();
		authpriv = -1;
		masterverdict = DISC_NONE;
	}

	void zap()
	{
		type = ST_EMPTY;
		priv = PRIV_NONE;
		authpriv = -1;
		masterverdict = DISC_NONE;
		connected = connectauth = haswelcome = false;
	}
};

struct ban
{
	enet_uint32 host;
	int millis;
};

struct server_entity			// server side version of "entity" type
{
	int type;
	bool spawned, hascoord;
	int spawntime;
	short x, y;
};

struct sknife{
	int id, millis;
	vec o;
};

struct demofile
{
    string info;
    uchar *data;
    int len;
};

void clearai(), checkai();
//void startgame(const char *newname, int newmode, int newtime = -1, bool notify = true);
void resetmap(const char *newname, int newmode, int newtime = -1, bool notify = true);
void disconnect_client(int n, int reason = -1);
int clienthasflag(int cn);
bool updateclientteam(int client, int team, int ftr);
bool refillteams(bool now = false, int ftr = FTR_AUTOTEAM);
void setpriv(int client, int priv);
int mapavailable(const char *mapname);
void getservermap(void);
// int findmappath(const char *mapname, char *filename = NULL);
mapstats *getservermapstats(const char *mapname, bool getlayout = false);
void sendf(int cn, int chan, const char *format, ...);

int explosion(client &owner, const vec &o2, int weap, bool gib = true);
/*
int calcscores();
void recordpacket(int chan, void *data, int len);
void senddisconnectedscores(int cn);
void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(packetbuf &p, int n);
void sendwelcome(client *cl, int chan = 1);
void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1);
int numclients();
void forcedeath(client *cl);

extern bool isdedicated;
extern string smapname;
extern mapstats smapstats;
extern char *maplayout;
*/

#ifdef _DEBUG
// converts message code to char
static const char *messagenames(int n){
	const char *msgnames[N_NUM] = {
		"N_SERVINFO", "N_WELCOME", "N_CONNECT", // before connection
		"N_INITCLIENT", "N_INITAI", "N_SETTEAM", "N_RESUME", "N_MAPIDENT", "N_DISC", "N_DELAI", "N_REASSIGNAI", // sent after (dis)connection
		"N_CLIENT", "N_POS", "N_PHYS", "N_PINGPONG", "N_PINGTIME", // automatic from client
		"N_TEXT", "N_WHOIS", "N_WHOISINFO", "N_NEWNAME", "N_SKIN", "N_LEVELUP", "N_SWITCHTEAM", // user-initiated
		"N_CALLVOTE", "N_CALLVOTEERR", "N_VOTE", "N_VOTERESULT", // votes
		"N_LISTDEMOS", "N_DEMO", "N_DEMOPLAYBACK", // demos
		"N_AUTHREQ", "N_AUTHCHAL", // auth
		"N_REQPRIV", "N_SETPRIV", // privileges
		"N_MAPC2S", "N_MAPS2C", // map transit
		// editmode ONLY
		"N_EDITMODE", "N_EDITH", "N_EDITT", "N_EDITS", "N_EDITD", "N_EDITE", "N_EDITW", "N_EDITENT", "N_NEWMAP",
		// game events
		"N_SHOOT", "N_SHOOTC", "N_PROJ", "N_AKIMBO", "N_RELOAD", // clients to server events
		"N_SG", "N_SCOPE", "N_SUICIDE", "N_QUICKSWITCH", "N_SWITCHWEAP", "N_LOADOUT", "N_THROWNADE", "N_THROWKNIFE", // server directly handled
		"N_RICOCHET", "N_POINTS", "N_KILL", "N_DAMAGE", "N_REGEN", "N_HEAL", "N_KNIFEADD", "N_KNIFEREMOVE", "N_BLEED", "N_STICK", "N_STREAKREADY", "N_STREAKUSE", // server to client
		// gameplay
		"N_TRYSPAWN", "N_SPAWNSTATE", "N_SPAWN", "N_FORCEDEATH", "N_FORCEGIB", // spawning
		"N_ITEMSPAWN", "N_ITEMACC", // items
		"N_DROPFLAG", "N_FLAGINFO", "N_FLAGMSG", "N_FLAGCNT", // flags
		"N_MAPCHANGE", "N_NEXTMAP", // map changes
		"N_TIMEUP", "N_ACCURACY", "N_ARENAWIN", // round end/remaining
		// extensions
		"N_SERVMSG", "N_CONFMSG", "N_EXT",
	};
	if(n < 0 || n >= N_NUM) return "unknown";
	return msgnames[n];
}
#endif

const char *entnames[MAXENTTYPES + 1] =
{
	"none?", "light", "playerstart",
	"pistol", "ammobox","grenades",
	"health", "armor", "akimbo",
	"mapmodel", "trigger",
	"ladder", "ctf-flag",
	"sound", "clip", "max",
};

itemstat ammostats[WEAP_MAX] =
{
	{1,  1,   2,	S_ITEMAMMO },   // knife dummy
	{24, 60,  72,	S_ITEMAMMO },   // pistol
	{21, 28,  42,	S_ITEMAMMO },   // shotgun
	{96, 128,  192,	S_ITEMAMMO },   // subgun
	{30, 40,  80,	S_ITEMAMMO },   // sniper
	{16, 24,  32,	S_ITEMAMMO },   // bolt sniper
	{90, 120,  180,	S_ITEMAMMO },   // assault
	{1,  1,   3,	S_ITEMAMMO },   // grenade
	{96, 0,   144,	S_ITEMAKIMBO },  // akimbo
	{40, 60,  80,	S_ITEMAMMO },   // heal
	{1,  1,   1,    S_ITEMAMMO }, // sword dummy
	{2,  3,   5,    S_ITEMAMMO }, // crossbow
};

itemstat powerupstats[] =
{
	{35, STARTHEALTH, MAXHEALTH, S_ITEMHEALTH}, //health
	{40, STARTARMOR, MAXARMOR, S_ITEMarmor}, //armor
};

guninfo guns[WEAP_MAX] =
{
//	{ modelname;     snd,	  rldsnd,  rldtime, atkdelay,  dmg, rngstart, rngend, rngm,psd,ptt,spr,kick,magsz,mkrot,mkback,rcoil,maxrcl,rca,pushf; auto;}
	{ "knife",      S_KNIFE,    S_ITEMAMMO,    0,   500,    80,    3,    4,   80,   0,   0,  1,    1,    1,   0,  0,     0,    0,       0, 5,   true },
	{ "pistol",     S_PISTOL,   S_RPISTOL,  1400,   90,     40,   40,  120,   17,   0,   0, 90,    9,   12,   6,  2,    32,    48,     70, 1,   false},
	{ "shotgun",    S_SHOTGUN,  S_RSHOTGUN,  750,   200,    10,    8,   16,    5,   0,   0,  1,   12,    7,   9,  5,    60,    70,      5, 2,   false},
	{ "subgun",     S_SUBGUN,   S_RSUBGUN,  2400,   67,     40,   32,   80,   22,   0,   0, 70,    4,   32,   1,  3,    23,    45,     65, 1,   true },
	{ "sniper",     S_SNIPER,   S_RSNIPER,  2000,   100,   120,    1,    2,   50,   0,   0,240,   14,   10,   4,  4,    58,    64,     75, 2,   false},
	{ "bolt",       S_BOLT,     S_RBOLT,    2000,   1500,  134,   80,  180,   34,   0,   0,260,   36,    8,   4,  4,    86,    90,     80, 3,   false},
	{ "assault",    S_ASSAULT,  S_RASSAULT, 2100,   73,     32,   40,  100,   12,   0,   0, 60,    3,   30,   0,  3,    24,    38,     60, 1,   true },
	{ "grenade",    S_NULL,     S_NULL,     1000,   650,   300,    0,   32,  270,  20,   6,  1,    1,    1,   3,  1,     0,    0,       0, 4,   false},
	{ "pistol",     S_PISTOL,   S_RAKIMBO,  1400,   80,     40,   45,  160,   17,   0,   0, 56,    8,   24,   6,  2,    28,    48,     70, 2,   true },
	{ "heal",       S_SUBGUN,   S_NULL,     1200,   100,    20,    4,    8,   10,   0,   0, 62,    1,   10,   0,  0,    10,    20,      8, 5,   true },
	{ "sword",      S_NULL,     S_RASSAULT,    0,   400,    90,    4,    7,   90,   0,   0,  1,    1,    1,   0,  2,     0,     0,      0, 0,   true },
	{ "bow",        S_NULL,     S_RASSAULT, 2000,   120,   250,    0,   24,  240,   0,   0, 88,    3,    1,   3,  1,    48,    50,      0, 4,   false},
};
