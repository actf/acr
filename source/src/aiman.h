// server-side ai (bot) manager
int findaiclient(int exclude = -1){ // person with least bots
	int cn = -1, bots = MAXCLIENTS;
	loopv(clients){
		client *c = clients[i];
		if(i == exclude || !valid_client(i, true) || c->clientnum < 0 /*|| !*c->name || !c->connected*/) break;
		int n = 0;
		loopvj(clients) if(clients[j]->state.ownernum == i) n++;
		if(n < bots || cn < 0){
			bots = n;
			cn = i;
		}
	}
	return cn;
}

bool addai(){
	int aiowner = findaiclient(), cn = -1, numbots = 0;
	if(!valid_client(aiowner)) return false;
	loopv(clients){
		if(numbots > (m_zombies ? MAXCLIENTS : MAXBOTS)) return false;
		if(clients[i]->type == ST_AI) ++numbots;
		else if(clients[i]->type == ST_EMPTY){
			cn = i;
			break;
		}
	}
	if(cn < 0){
		client *c = new client;
		c->clientnum = cn = clients.length();
		clients.add(c);
	}
	client &b = *clients[cn];
	b.reset();
	formatstring(b.name)("bot%d-%d", cn, aiowner);
	b.type = ST_AI;
	b.connected = true;
	b.state.level = 60 + rnd(36); // how smart/stupid the bot is can be set here (currently random from 60 to 95)
	sendf(-1, 1, "ri6", N_INITAI, cn, (b.team = freeteam(cn)), (b.skin = rand()), b.state.level, (b.state.ownernum = aiowner));
	forcedeath(&b);
	return true;
}

void deleteai(client &c){
    if(c.type != ST_AI || c.state.ownernum < 0) return;
    const int cn = c.clientnum;
	loopv(clients) if(i != cn){
		clients[i]->state.damagelog.removeobj(cn);
		clients[i]->state.revengelog.removeobj(cn);
	}
	sdropflag(cn);
	if(c.priv) setpriv(cn, PRIV_NONE);
	c.state.ownernum = -1;
	c.zap();
	sendf(-1, 1, "ri2", N_DELAI, cn);
}

bool delai(){
	loopvrev(clients) if(clients[i]->type == ST_AI){
		deleteai(*clients[i]);
		return true;
	}
	return false;
}

bool shiftai(client &c, int ncn = -1, int exclude = -1){
	if(!valid_client(ncn, true)){
		ncn = findaiclient(exclude);
		if(!valid_client(ncn, true)) return false;
	}
	formatstring(c.name)("bot%d-%d", c.clientnum, ncn);
	sendf(-1, 1, "ri3", N_REASSIGNAI, c.clientnum, c.state.ownernum = ncn);
	return true;
}

void clearai(){ loopv(clients) if(clients[i]->type == ST_AI) deleteai(*clients[i]); }

bool reassignai(int exclude = -1){
	int hi = -1, lo = -1, hicount = -1, locount = -1;
	loopv(clients)
	{
		client *ci = clients[i];
		if(ci->type == ST_EMPTY || ci->type == ST_AI || !ci->name[0] || !ci->connected || ci->clientnum == exclude) continue;
		int thiscount = 0;
		loopvj(clients) if(clients[j]->state.ownernum == ci->clientnum) ++thiscount;
		if(hi < 0 || thiscount > hicount){ hi = i; hicount = thiscount; }
		if(lo < 0 || thiscount < locount){ lo = i; locount = thiscount; }
	}
	if(hi >= 0 && lo >= 0 && hicount > locount + 1)
	{
		client *ci = clients[hi];
		loopv(clients) if(clients[i]->type == ST_AI && clients[i]->state.ownernum == ci->clientnum) return shiftai(*clients[i], lo);
	}
	return false;
}

void checkai(){
	// check if bots are disallowed
	if(!m_ai) return clearai();
	// check balance
	int balance = 0;
	const int people = numclients();
	if(!botbalance) balance = 0;
	else if(people) switch(botbalance){
		case -1: // auto
			if(m_zombies) balance = 12 + 3 * people; // effectively 12 + 2n
			else if(m_duel) balance = max(people, maplayout_factor - 3); // 3 - 5 - 8 (6 - 8 - 11 layout factor)
			else{
				const int spawns = m_team ? (smapstats.hasteamspawns ? smapstats.spawns[0] + smapstats.spawns[1] : 16) : (smapstats.hasffaspawns ? smapstats.spawns[2] : 6);
				balance = max(people, spawns / 3);
				if(balance % 1 && m_team) ++balance;
			}
			break; // auto
		//case  0: balance = 0; break; // force no bots
		default: balance = max(people, botbalance); break; // force bot count
	}
	if(balance > 0){
		if(m_team && !m_zombies){
			int plrs[2] = {0}, highest = -1;
			loopv(clients) if(valid_client(i, true) && clients[i]->team < 2){
				plrs[clients[i]->team]++;
				if(highest < 0 || plrs[clients[i]->team] > plrs[highest]) highest = clients[i]->team;
			}
			if(highest >= 0){
				int bots = balance-people;
				loopi(2) if(i != highest && plrs[i] < plrs[highest]) loopj(plrs[highest]-plrs[i]){
					if(bots > 0) bots--;
					else balance++;
				}
			}
		}
		while(countplayers() < balance) if(!addai()) break;
		while(countplayers() > balance) if(!delai()) break;
	}
	else clearai();
}