//
// C++ Implementation: bot
//
// Description: Code for botmanager
//
// Main bot file
//
// Author:  Rick <rickhelmus@gmail.com>
//
//
//

#include "pch.h"
#include "bot.h"

extern void respawnself();

CBotManager BotManager;

// Bot manager class begin

CBotManager::~CBotManager(void){  }

void CBotManager::Init()
{
	 m_bBotsShoot = true;
	 m_bIdleBots = false;
	 m_iFrameTime = 0;
	 m_iPrevTime = lastmillis;
	 m_sBotSkill = 1; // Default all bots have the skill 'Good'
	 
	 CreateSkillData();
	 LoadBotNamesFile();
	 LoadBotTeamsFile();
	 //WaypointClass.Init();
	 lsrand(time(NULL));
}

CACBot *botcontrollers[MAXCLIENTS] = { NULL }; // reuse bot controllers

void CBotManager::Think()
{	
	 if (m_bInit)
	 {
		  Init();
		  m_bInit = false;
	 }

	 AddDebugText("m_sMaxAStarBots: %d", m_sMaxAStarBots);
	 AddDebugText("m_sCurrentTriggerNr: %d", m_sCurrentTriggerNr);
	 short x, y;
	 WaypointClass.GetNodeIndexes(player1->o, &x, &y);
	 AddDebugText("x: %d y: %d", x, y);
	 
	 m_iFrameTime = lastmillis - m_iPrevTime;
	 if (m_iFrameTime > 250) m_iFrameTime = 250;
	 m_iPrevTime = lastmillis;

	// Added by Victor: control multiplayer bots
	const int ourcn = getclientnum();
	if(ourcn >= 0){
		// handle the bots
		loopv(players){
			if(!players[i] || players[i]->ownernum != ourcn) continue;
			playerent *b = players[i];
			#define bc b->pBot // this is the bot controller
			if(!bc){
				if(botcontrollers[i]) bc = botcontrollers[i];
				else bc = botcontrollers[i] = new CACBot();
				// m->type = ENT_BOT;
				bc->m_pMyEnt = b;
				// m->pBot->m_iLastBotUpdate = 0;
				// m->pBot->m_bSendC2SInit = false;
				bc->m_sSkillNr = BotManager.m_sBotSkill;
				bc->m_pBotSkill = &BotManager.m_BotSkills[bc->m_sSkillNr];

				// Sync waypoints
				bc->SyncWaypoints();
				// Try spawn
				bc->Spawn();
			}
			bc->Think();
		}
	}
}

void CBotManager::LoadBotNamesFile()
{
	 // Init bot names array first
	 for (int i=0;i<100;i++)
		  strcpy(m_szBotNames[i], "Bot");
	 
	 m_sBotNameCount = 0;
	 
	 // Load bot file
	 char szNameFileName[256];
	 MakeBotFileName("bot_names.txt", NULL, NULL, szNameFileName);
	 FILE *fp = fopen(szNameFileName, "r");
	 char szNameBuffer[256];
	 int iIndex, iStrIndex;
	 
	 if (!fp)
	 {
		  conoutf("Warning: Couldn't load bot names file");
		  return;
	 }
	 
	 while (fgets(szNameBuffer, 80, fp) != NULL)
	 {
		  if (m_sBotNameCount >= 150)
		  {
			   conoutf("Warning: Max bot names reached(150), ignoring the rest of the"
					   "names");
			   break;
		  }
		  
		  short length = (short)strlen(szNameBuffer);

		  if (szNameBuffer[length-1] == '\n')
		  {
			   szNameBuffer[length-1] = 0;  // remove '\n'
			   length--;
		  }

		  iStrIndex = 0;
		  while (iStrIndex < length)
		  {
			   if ((szNameBuffer[iStrIndex] < ' ') || (szNameBuffer[iStrIndex] > '~') ||
				   (szNameBuffer[iStrIndex] == '"'))
			   {
					for (iIndex=iStrIndex; iIndex < length; iIndex++)
						 szNameBuffer[iIndex] = szNameBuffer[iIndex+1];
			   }
 
			   iStrIndex++;
		  }

		  if (szNameBuffer[0] != 0)
		  {
			   if (strlen(szNameBuffer) >= 16)
			   {	 conoutf("Warning: bot name \"%s\" has to many characters(16 is max)",
							szNameBuffer);
			   }
			   copystring(m_szBotNames[m_sBotNameCount], szNameBuffer, 16);
			   m_sBotNameCount++;
		  }
	 }
	 fclose(fp);
}
	  
void CBotManager::LoadBotTeamsFile()
{
	 // Init bot teams array first
	 for (int i=0;i<20;i++)
		  strcpy(m_szBotTeams[i], "b0ts");
	 
	 m_sBotTeamCount = 0;
	 
	 // Load bot file
	 char szNameFileName[256];
	 MakeBotFileName("bot_teams.txt", NULL, NULL, szNameFileName);
	 FILE *fp = fopen(szNameFileName, "r");
	 char szNameBuffer[256];
	 int iIndex, iStrIndex;
	 
	 if (!fp)
	 {
		  conoutf("Warning: Couldn't load bot teams file");
		  return;
	 }
	 
	 while ((m_sBotTeamCount < 20) && (fgets(szNameBuffer, 80, fp) != NULL))
	 {
		  short length = (short)strlen(szNameBuffer);

		  if (szNameBuffer[length-1] == '\n')
		  {
			   szNameBuffer[length-1] = 0;  // remove '\n'
			   length--;
		  }

		  iStrIndex = 0;
		  while (iStrIndex < length)
		  {
			   if ((szNameBuffer[iStrIndex] < ' ') || (szNameBuffer[iStrIndex] > '~') ||
				   (szNameBuffer[iStrIndex] == '"'))
			   {
					for (iIndex=iStrIndex; iIndex < length; iIndex++)
						 szNameBuffer[iIndex] = szNameBuffer[iIndex+1];
			   }
 
			   iStrIndex++;
		  }

		  if (szNameBuffer[0] != 0)
		  {
			   copystring(m_szBotTeams[m_sBotTeamCount], szNameBuffer, 5);
			   m_sBotTeamCount++;
		  }
	 }
	 fclose(fp);
}

const char *CBotManager::GetBotTeam()
{
	 const char *szOutput = NULL;
	 TMultiChoice<const char *> BotTeamChoices;
	 short ChoiceVal;
	 
	 for(int j=0;j<m_sBotTeamCount;j++)
	 {
		  ChoiceVal = 50;
		  /* UNDONE?
		  loopv(players)
		  {
			   if (players[i] && (!strcasecmp(players[i]->name, m_szBotNames[j])))
					ChoiceVal -= 10;
		  }
		  
		  loopv(bots)
		  {
			   if (bots[i] && (!strcasecmp(bots[i]->name, m_szBotNames[j])))
					ChoiceVal -= 10;
		  }
		  
		  if (!strcasecmp(player1->name, m_szBotNames[j]))
			   ChoiceVal -= 10;
			   
		  if (ChoiceVal <= 0)
			   ChoiceVmonsterclearal = 1;*/
		  
		  BotTeamChoices.Insert(m_szBotTeams[j], ChoiceVal);
	 }
	 
	 // Couldn't find a selection?
	 if (!BotTeamChoices.GetSelection(szOutput))
		  szOutput = "b0t";
	 
	 return szOutput;
}

void CBotManager::BeginMap(const char *szMapName)
{ 
	 WaypointClass.Init();
	 WaypointClass.SetMapName(szMapName);
	 if (*szMapName && !WaypointClass.LoadWaypoints())
		  WaypointClass.StartFlood();
	 //WaypointClass.LoadWPExpFile(); // UNDONE

	 CalculateMaxAStarCount();
	 m_sUsingAStarBotsCount = 0;
	 PickNextTrigger();
}

void CBotManager::LetBotsHear(int n, vec *loc)
{
	 if (!loc) return;
		  
	 loopv(players)
	 {
		  if (!players[i] || players[i]->ownernum < 0 || !players[i]->pBot || players[i]->state == CS_DEAD) continue;
		  players[i]->pBot->HearSound(n, loc);
	 }
}

// Notify all bots of a new waypoint
void CBotManager::AddWaypoint(node_s *pNode)
{

	 CalculateMaxAStarCount();
}

// Notify all bots of a deleted waypoint
void CBotManager::DelWaypoint(node_s *pNode)
{
	 CalculateMaxAStarCount();
}

void CBotManager::MakeBotFileName(const char *szFileName, const char *szDir1, const char *szDir2, char *szOutput)
{
	 const char *DirSeperator;

#ifdef WIN32
	 DirSeperator = "\\";
	 strcpy(szOutput, "bot\\");
#else
	 DirSeperator = "/";
	 strcpy(szOutput, "bot/");
#endif
	 
	 if (szDir1)
	 {
		  strcat(szOutput, szDir1);
		  strcat(szOutput, DirSeperator);
	 }
	 
	 if (szDir2)
	 {
		  strcat(szOutput, szDir2);
		  strcat(szOutput, DirSeperator);
	 }
	 
	 strcat(szOutput, szFileName);
}

void CBotManager::CreateSkillData()
{
	 // First give the bot skill structure some default data
	 InitSkillData();
	 
	 // Now see if we can load the skill.cfg file
	 char SkillFileName[256] = "";
	 FILE *pSkillFile = NULL;
	 int SkillNr = -1;
	 float value = 0;

	 MakeBotFileName("bot_skill.cfg", NULL, NULL, SkillFileName);

	 pSkillFile = fopen(SkillFileName, "r");

	 conoutf("Reading bot_skill.cfg file... ");

	 if (pSkillFile == NULL) // file doesn't exist
	 {
		  conoutf("skill file not found, default settings will be used\n");
		  return;
	 }

	 int ch;
	 char cmd_line[256];
	 int cmd_index;
	 char *cmd, *arg1;

	 while (pSkillFile)
	 {	
		  cmd_index = 0;
		  cmd_line[cmd_index] = 0;

		  ch = fgetc(pSkillFile);

		  // skip any leading blanks
		  while (ch == ' ')
			   ch = fgetc(pSkillFile);

		  while ((ch != EOF) && (ch != '\r') && (ch != '\n'))
		  {
			   if (ch == '\t')  // convert tabs to spaces
					ch = ' ';

			   cmd_line[cmd_index] = ch;

			   ch = fgetc(pSkillFile);

			   // skip multiple spaces in input file
			   while ((cmd_line[cmd_index] == ' ') && (ch == ' '))	  
					ch = fgetc(pSkillFile);

			   cmd_index++;
		  }

		  if (ch == '\r')  // is it a carriage return?
		  {
			   ch = fgetc(pSkillFile);  // skip the linefeed
		  }

		  // if reached end of file, then close it
		  if (ch == EOF)
		  {
			   fclose(pSkillFile);
			   pSkillFile = NULL;
		  }

		  cmd_line[cmd_index] = 0;  // terminate the command line

		  cmd_index = 0;
		  cmd = cmd_line;
		  arg1 = NULL;

		  // skip to blank or end of string...
		  while ((cmd_line[cmd_index] != ' ') && (cmd_line[cmd_index] != 0))
				cmd_index++;

		  if (cmd_line[cmd_index] == ' ')
		  {
				cmd_line[cmd_index++] = 0;
				arg1 = &cmd_line[cmd_index];
		  }

		  if ((cmd_line[0] == '#') || (cmd_line[0] == 0))
			   continue;  // skip if comment or blank line


		  if (strcasecmp(cmd, "[SKILL1]") == 0)
			   SkillNr = 0;
		  else if (strcasecmp(cmd, "[SKILL2]") == 0)
			   SkillNr = 1;
		  else if (strcasecmp(cmd, "[SKILL3]") == 0)
			   SkillNr = 2;
		  else if (strcasecmp(cmd, "[SKILL4]") == 0)
			   SkillNr = 3;
		  else if (strcasecmp(cmd, "[SKILL5]") == 0)
			   SkillNr = 4;

		  if ((arg1 == NULL) || (*arg1 == 0))
			  continue;

		  if (SkillNr == -1) // Not in a skill block yet?
			   continue;

		  value = atof(arg1);

		  if (strcasecmp(cmd, "min_x_aim_speed") == 0)
		  {
			   m_BotSkills[SkillNr].flMinAimXSpeed = value;
		  }
		  else if (strcasecmp(cmd, "max_x_aim_speed") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxAimXSpeed = value;
		  }
		  else if (strcasecmp(cmd, "min_y_aim_speed") == 0)
		  {
			   m_BotSkills[SkillNr].flMinAimYSpeed = value;
		  }
		  else if (strcasecmp(cmd, "max_y_aim_speed") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxAimYSpeed = value;
		  }
		  else if (strcasecmp(cmd, "min_x_aim_offset") == 0)
		  {
			   m_BotSkills[SkillNr].flMinAimXOffset = value;
		  }
		  else if (strcasecmp(cmd, "max_x_aim_offset") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxAimXOffset = value;
		  }
		  else if (strcasecmp(cmd, "min_y_aim_offset") == 0)
		  {
			   m_BotSkills[SkillNr].flMinAimYOffset = value;
		  }
		  else if (strcasecmp(cmd, "max_y_aim_offset") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxAimYOffset = value;
		  }
		  else if (strcasecmp(cmd, "min_attack_delay") == 0)
		  {
			   m_BotSkills[SkillNr].flMinAttackDelay = value;
		  }
		  else if (strcasecmp(cmd, "max_attack_delay") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxAttackDelay = value;
		  }
		  else if (strcasecmp(cmd, "min_enemy_search_delay") == 0)
		  {
			   m_BotSkills[SkillNr].flMinEnemySearchDelay = value;
		  }
		  else if (strcasecmp(cmd, "max_enemy_search_delay") == 0)
		  {
			   m_BotSkills[SkillNr].flMaxEnemySearchDelay = value;
		  }
		  else if (strcasecmp(cmd, "shoot_at_feet_percent") == 0)
		  {
			   if (value < 0) value = 0;
			   else if (value > 100) value = 100;
			   m_BotSkills[SkillNr].sShootAtFeetWithRLPercent = (short)value;
		  }
		  else if (strcasecmp(cmd, "can_predict_position") == 0)
		  {
			   m_BotSkills[SkillNr].bCanPredict = value!=0;
		  }
		  else if (strcasecmp(cmd, "max_hear_volume") == 0)
		  {
			   if (value < 0) value = 0;
			   else if (value > 255) value = 100;
			   m_BotSkills[SkillNr].iMaxHearVolume = (int)value;
		  }
		  else if (strcasecmp(cmd, "can_circle_strafe") == 0)
		  {
			   m_BotSkills[SkillNr].bCircleStrafe = value!=0;
		  }	 
		  else if (strcasecmp(cmd, "can_search_items_in_combat") == 0)
		  {
			   m_BotSkills[SkillNr].bCanSearchItemsInCombat = value!=0;
		  }		  
	 }

	 conoutf("done");
}

void CBotManager::InitSkillData()
{
	 // Best skill
	 m_BotSkills[0].flMinReactionDelay = 0.015f;
	 m_BotSkills[0].flMaxReactionDelay = 0.035f;
	 m_BotSkills[0].flMinAimXOffset = 15.0f;
	 m_BotSkills[0].flMaxAimXOffset = 20.0f;
	 m_BotSkills[0].flMinAimYOffset = 10.0f;
	 m_BotSkills[0].flMaxAimYOffset = 15.0f;
	 m_BotSkills[0].flMinAimXSpeed = 330.0f;
	 m_BotSkills[0].flMaxAimXSpeed = 355.0f;
	 m_BotSkills[0].flMinAimYSpeed = 400.0f;
	 m_BotSkills[0].flMaxAimYSpeed = 450.0f;
	 m_BotSkills[0].flMinAttackDelay = 0.1f;
	 m_BotSkills[0].flMaxAttackDelay = 0.4f;
	 m_BotSkills[0].flMinEnemySearchDelay = 0.09f;
	 m_BotSkills[0].flMaxEnemySearchDelay = 0.12f;
	 m_BotSkills[0].sShootAtFeetWithRLPercent = 85;
	 m_BotSkills[0].bCanPredict = true;
	 m_BotSkills[0].iMaxHearVolume = 75;
	 m_BotSkills[0].bCircleStrafe = true;
	 m_BotSkills[0].bCanSearchItemsInCombat = true;

	 // Good skill
	 m_BotSkills[1].flMinReactionDelay = 0.035f;
	 m_BotSkills[1].flMaxReactionDelay = 0.045f;
	 m_BotSkills[1].flMinAimXOffset = 20.0f;
	 m_BotSkills[1].flMaxAimXOffset = 25.0f;
	 m_BotSkills[1].flMinAimYOffset = 15.0f;
	 m_BotSkills[1].flMaxAimYOffset = 20.0f;
	 m_BotSkills[1].flMinAimXSpeed = 250.0f;
	 m_BotSkills[1].flMaxAimXSpeed = 265.0f;
	 m_BotSkills[1].flMinAimYSpeed = 260.0f;
	 m_BotSkills[1].flMaxAimYSpeed = 285.0f;
	 m_BotSkills[1].flMinAttackDelay = 0.3f;
	 m_BotSkills[1].flMaxAttackDelay = 0.6f;
	 m_BotSkills[1].flMinEnemySearchDelay = 0.12f;
	 m_BotSkills[1].flMaxEnemySearchDelay = 0.17f;
	 m_BotSkills[1].sShootAtFeetWithRLPercent = 60;
	 m_BotSkills[1].bCanPredict = true;
	 m_BotSkills[1].iMaxHearVolume = 60;
	 m_BotSkills[1].bCircleStrafe = true;
	 m_BotSkills[1].bCanSearchItemsInCombat = true;

	 // Medium skill
	 m_BotSkills[2].flMinReactionDelay = 0.075f;
	 m_BotSkills[2].flMaxReactionDelay = 0.010f;
	 m_BotSkills[2].flMinAimXOffset = 25.0f;
	 m_BotSkills[2].flMaxAimXOffset = 30.0f;
	 m_BotSkills[2].flMinAimYOffset = 20.0f;
	 m_BotSkills[2].flMaxAimYOffset = 25.0f;
	 m_BotSkills[2].flMinAimXSpeed = 190.0f;
	 m_BotSkills[2].flMaxAimXSpeed = 125.0f;
	 m_BotSkills[2].flMinAimYSpeed = 210.0f;
	 m_BotSkills[2].flMaxAimYSpeed = 240.0f;
	 m_BotSkills[2].flMinAttackDelay = 0.75f;
	 m_BotSkills[2].flMaxAttackDelay = 1.0f;
	 m_BotSkills[2].flMinEnemySearchDelay = 0.18f;
	 m_BotSkills[2].flMaxEnemySearchDelay = 0.22f;
	 m_BotSkills[2].sShootAtFeetWithRLPercent = 25;
	 m_BotSkills[2].bCanPredict = false;
	 m_BotSkills[2].iMaxHearVolume = 45;
	 m_BotSkills[2].bCircleStrafe = true;
	 m_BotSkills[2].bCanSearchItemsInCombat = false;

	 // Worse skill
	 m_BotSkills[3].flMinReactionDelay = 0.15f;
	 m_BotSkills[3].flMaxReactionDelay = 0.20f;
	 m_BotSkills[3].flMinAimXOffset = 30.0f;
	 m_BotSkills[3].flMaxAimXOffset = 35.0f;
	 m_BotSkills[3].flMinAimYOffset = 25.0f;
	 m_BotSkills[3].flMaxAimYOffset = 30.0f;
	 m_BotSkills[3].flMinAimXSpeed = 155.0f;
	 m_BotSkills[3].flMaxAimXSpeed = 170.0f;
	 m_BotSkills[3].flMinAimYSpeed = 160.0f;
	 m_BotSkills[3].flMaxAimYSpeed = 210.0f;
	 m_BotSkills[3].flMinAttackDelay = 1.2f;
	 m_BotSkills[3].flMaxAttackDelay = 1.6f;
	 m_BotSkills[3].flMinEnemySearchDelay = 0.25f;
	 m_BotSkills[3].flMaxEnemySearchDelay = 0.30f;
	 m_BotSkills[3].sShootAtFeetWithRLPercent = 10;
	 m_BotSkills[3].bCanPredict = false;
	 m_BotSkills[3].iMaxHearVolume = 30;
	 m_BotSkills[3].bCircleStrafe = false;
	 m_BotSkills[3].bCanSearchItemsInCombat = false;

	 // Bad skill
	 m_BotSkills[4].flMinReactionDelay = 0.30f;
	 m_BotSkills[4].flMaxReactionDelay = 0.50f;
	 m_BotSkills[4].flMinAimXOffset = 35.0f;
	 m_BotSkills[4].flMaxAimXOffset = 40.0f;
	 m_BotSkills[4].flMinAimYOffset = 30.0f;
	 m_BotSkills[4].flMaxAimYOffset = 35.0f;
	 m_BotSkills[4].flMinAimXSpeed = 45.0f;
	 m_BotSkills[4].flMaxAimXSpeed = 60.0f;
	 m_BotSkills[4].flMinAimYSpeed = 125.0f;
	 m_BotSkills[4].flMaxAimYSpeed = 180.0f;
	 m_BotSkills[4].flMinAttackDelay = 1.5f;
	 m_BotSkills[4].flMaxAttackDelay = 2.0f;
	 m_BotSkills[4].flMinEnemySearchDelay = 0.30f;
	 m_BotSkills[4].flMaxEnemySearchDelay = 0.36f;
	 m_BotSkills[4].sShootAtFeetWithRLPercent = 0;
	 m_BotSkills[4].bCanPredict = false;
	 m_BotSkills[4].iMaxHearVolume = 15;
	 m_BotSkills[4].bCircleStrafe = false;
	 m_BotSkills[4].bCanSearchItemsInCombat = false;
}

void CBotManager::CalculateMaxAStarCount()
{  
	 if (WaypointClass.m_iWaypointCount > 0) // Are there any waypoints?
	 {
		  m_sMaxAStarBots = 8 - short(ceil((float)WaypointClass.m_iWaypointCount /
									  1000.0f));
		  if (m_sMaxAStarBots < 1)
			   m_sMaxAStarBots = 1;
	 }
	 else
		  m_sMaxAStarBots = 1;
}

void CBotManager::PickNextTrigger()
{
	 short lowest = -1;
	 bool found0 = false; // True if found a trigger with nr 0
	 
	 loopv(ents)
	 {
		  entity &e = ents[i];
		  
#if defined AC_CUBE		  
/*		  if ((e.type != TRIGGER) || !e.spawned)
			   continue;*/
#elif defined VANILLA_CUBE
		  if ((e.type != CARROT) || !e.spawned)
			   continue;
#endif		  
		  if (OUTBORD(e.x, e.y)) continue;
		  
		  vec o(e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight);

		  node_s *pWptNearEnt = NULL;
		  
		  pWptNearEnt = WaypointClass.GetNearestTriggerWaypoint(o, 2.0f);
		  
		  if (pWptNearEnt)
		  {
			   if ((pWptNearEnt->sTriggerNr > 0) &&
				   ((pWptNearEnt->sTriggerNr < lowest) || (lowest == -1)))
					lowest = pWptNearEnt->sTriggerNr;
			   if (pWptNearEnt->sTriggerNr == 0) found0 = true;
		  }
		  
#ifdef WP_FLOOD
		  pWptNearEnt = WaypointClass.GetNearestTriggerFloodWP(o, 2.0f);
		  
		  if (pWptNearEnt)
		  {
			   if ((pWptNearEnt->sTriggerNr > 0) && 
				   ((pWptNearEnt->sTriggerNr < lowest) || (lowest == -1)))
					lowest = pWptNearEnt->sTriggerNr;
			   if (pWptNearEnt->sTriggerNr == 0) found0 = true;
		  }
					
#endif			   
	 }
	 
	 if ((lowest == -1) && found0) lowest = 0;
	 
	 if (lowest != -1)
		  m_sCurrentTriggerNr = lowest;
}

// Bot manager class end

void togglegrap()
{
	 if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GrabMode(0)) SDL_WM_GrabInput(SDL_GRAB_ON);
	 else SDL_WM_GrabInput(SDL_GrabMode(0));
}

COMMAND(togglegrap, ARG_NONE);

#ifndef RELEASE_BUILD

#ifdef VANILLA_CUBE
void drawbeamtocarrots()
{
	 loopv(ents)
	 {
		  entity &e = ents[i];
		  vec o = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
		  if ((e.type != CARROT) || !e.spawned) continue;
		  particle_trail(1, 500, player1->o, o);
	 }
}

COMMAND(drawbeamtocarrots, ARG_NONE);

void drawbeamtoteleporters()
{
	 loopv(ents)
	 {
		  entity &e = ents[i];
		  vec o = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
		  if (e.type != TELEPORT) continue;
		  particle_trail(1, 500, player1->o, o);
	 }
}

COMMAND(drawbeamtoteleporters, ARG_NONE);
#endif

void testvisible(int iDir)
{
	
	 vec angles, end, forward, right, up;
	 traceresult_s tr;
	 int Dir;
	 
	 switch(iDir)
	 {
		  case 0: default: Dir = FORWARD; break;
		  case 1: Dir = BACKWARD; break;
		  case 2: Dir = LEFT; break;
		  case 3: Dir = RIGHT; break;
		  case 4: Dir = UP; break;
		  case 5: Dir = DOWN; break;
	 }
	 
	 vec from = player1->o;
	 from.z -= (player1->eyeheight - 1.25f);
	 end = from;
	 makevec(&angles, player1->pitch, player1->yaw, player1->roll);
	 angles.x=0;
	 
	 if (Dir & UP)
		  angles.x = WrapXAngle(angles.x + 45.0f);
	 else if (Dir & DOWN)
		  angles.x = WrapXAngle(angles.x - 45.0f);
		  
	 if ((Dir & FORWARD) || (Dir & BACKWARD))
	 {
		  if (Dir & BACKWARD)
			   angles.y = WrapYZAngle(angles.y + 180.0f);
		  
		  if (Dir & LEFT)
		  {
			   if (Dir & FORWARD)
					angles.y = WrapYZAngle(angles.y - 45.0f);
			   else
					angles.y = WrapYZAngle(angles.y + 45.0f);
		  }
		  else if (Dir & RIGHT)
		  {
			   if (Dir & FORWARD)
					angles.y = WrapYZAngle(angles.y + 45.0f);
			   else
					angles.y = WrapYZAngle(angles.y - 45.0f);
		  }
	 }
	 else if (Dir & LEFT)
		  angles.y = WrapYZAngle(angles.y - 90.0f);
	 else if (Dir & RIGHT)
		  angles.y = WrapYZAngle(angles.y + 90.0f);
	 else if (Dir & UP)
		  angles.x = WrapXAngle(angles.x + 90.0f);
	 else if (Dir & DOWN)
		  angles.x = WrapXAngle(angles.x - 90.0f);
		  
	 AnglesToVectors(angles, forward, right, up);
	 
	 forward.mul(20.0f);
	 end.add(forward);
		 
	 TraceLine(from, end, player1, false, &tr);
	 
	 //debugbeam(from, tr.end);
	 char sz[250];
	 sprintf(sz, "dist: %f; hit: %d", GetDistance(from, tr.end), tr.collided);
	 condebug(sz);
}

COMMAND(testvisible, ARG_1INT);

void mapsize(void)
{
	 conoutf("ssize: %d", ssize);
}

COMMAND(mapsize, ARG_NONE);
			   
#endif
