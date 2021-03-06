/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//

/*****************************************************************************
 * name:		ai_cmd.c
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_cmd.c $
 *
 *****************************************************************************/

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
//
#include "ai_char.h"
#include "ai_chat_sys.h"
#include "ai_ea.h"
#include "ai_gen.h"
#include "ai_goal.h"
#include "ai_move.h"
#include "ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_vcmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
//
#include "chars.h"				//characteristics
#include "inv.h"				//indexes into the inventory
#include "syn.h"				//synonyms
#include "match.h"				//string matching types and vars

// for the voice chats
#include "../../ui/menudef.h"

int notleader[MAX_CLIENTS];

/*
==================
BotPrintTeamGoal
==================
*/
void BotPrintTeamGoal(bot_state_t *bs) {
	char netname[MAX_NETNAME];
	float t;

	if (!bot_showteamgoals.integer) {
		return;
	}

	PlayerName(bs->playernum, netname, sizeof(netname));
	t = bs->teamgoal_time - FloatTime();
	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna help a team mate for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna accompany a team mate for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna get the flag for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna rush to the base for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna try to return the flag for %1.0f secs\n", netname, t);
			break;
		}
#ifdef MISSIONPACK
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna attack the enemy base for %1.0f secs\n", netname, t);
			break;
		}
#ifdef MISSIONPACK_HARVESTER
		case LTG_HARVEST:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna harvest for %1.0f secs\n", netname, t);
			break;
		}
#endif
#endif
		case LTG_DEFENDKEYAREA:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna defend a key area for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_GETITEM:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna get an item for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_KILL:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna kill someone for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna camp for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna patrol for %1.0f secs\n", netname, t);
			break;
		}
		default:
		{
			if (bs->ctfroam_time > FloatTime()) {
				t = bs->ctfroam_time - FloatTime();
				BotAI_Print(PRT_MESSAGE, "%s: I'm gonna roam for %1.0f secs\n", netname, t);
			}
			else {
				BotAI_Print(PRT_MESSAGE, "%s: I've got a regular goal\n", netname);
			}
		}
	}
}

/*
==================
BotGetItemTeamGoal

FIXME: add stuff like "upper rocket launcher"
"the rl near the railgun", "lower grenade launcher" etc.
==================
*/
int BotGetItemTeamGoal(char *goalname, bot_goal_t *goal) {
	int i;

	if (!strlen(goalname)) return qfalse;
	i = -1;
	do {
		i = BotGetLevelItemGoal(i, goalname, goal);
		if (i > 0) {
			//do NOT defend dropped items
			if (goal->flags & GFL_DROPPED)
				continue;
			return qtrue;
		}
	} while(i > 0);
	return qfalse;
}

/*
==================
BotGetMessageTeamGoal
==================
*/
int BotGetMessageTeamGoal(bot_state_t *bs, char *goalname, bot_goal_t *goal) {
	bot_waypoint_t *cp;

	if (BotGetItemTeamGoal(goalname, goal)) return qtrue;

	cp = BotFindWayPoint(bs->checkpoints, goalname);
	if (cp) {
		memcpy(goal, &cp->goal, sizeof(bot_goal_t));
		return qtrue;
	}
	return qfalse;
}

/*
==================
BotGetTime
==================
*/
float BotGetTime(bot_match_t *match) {
	bot_match_t timematch;
	char timestring[MAX_MESSAGE_SIZE];
	float t;

	//if the matched string has a time
	if (match->subtype & ST_TIME) {
		//get the time string
		BotMatchVariable(match, TIME, timestring, MAX_MESSAGE_SIZE);
		//match it to find out if the time is in seconds or minutes
		if (BotFindMatch(timestring, &timematch, MTCONTEXT_TIME)) {
			if (timematch.type == MSG_FOREVER) {
				t = 99999999.0f;
			}
			else if (timematch.type == MSG_FORAWHILE) {
				t = 10 * 60; // 10 minutes
			}
			else if (timematch.type == MSG_FORALONGTIME) {
				t = 30 * 60; // 30 minutes
			}
			else {
				BotMatchVariable(&timematch, TIME, timestring, MAX_MESSAGE_SIZE);
				if (timematch.type == MSG_MINUTES) t = atof(timestring) * 60;
				else if (timematch.type == MSG_SECONDS) t = atof(timestring);
				else t = 0;
			}
			//if there's a valid time
			if (t > 0) return FloatTime() + t;
		}
	}
	return 0;
}

/*
==================
FindPlayerByName
==================
*/
int FindPlayerByName(char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < level.maxplayers; i++) {
		PlayerName(i, buf, sizeof(buf));
		if (!Q_stricmp(buf, name)) return i;
	}
	for (i = 0; i < level.maxplayers; i++) {
		PlayerName(i, buf, sizeof(buf));
		if (stristr(buf, name)) return i;
	}
	return -1;
}

/*
==================
FindEnemyByName
==================
*/
int FindEnemyByName(bot_state_t *bs, char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < level.maxplayers; i++) {
		if (BotSameTeam(bs, i)) continue;
		PlayerName(i, buf, sizeof(buf));
		if (!Q_stricmp(buf, name)) return i;
	}
	for (i = 0; i < level.maxplayers; i++) {
		if (BotSameTeam(bs, i)) continue;
		PlayerName(i, buf, sizeof(buf));
		if (stristr(buf, name)) return i;
	}
	return -1;
}

/*
==================
NumPlayersOnSameTeam
==================
*/
int NumPlayersOnSameTeam(bot_state_t *bs) {
	int i, num;
	char buf[MAX_INFO_STRING];

	num = 0;
	for (i = 0; i < level.maxplayers; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, MAX_INFO_STRING);
		if (strlen(buf)) {
			if (BotSameTeam(bs, i+1)) num++;
		}
	}
	return num;
}

/*
==================
TeamPlayIsOn
==================
*/
int BotGetPatrolWaypoints(bot_state_t *bs, bot_match_t *match) {
	char keyarea[MAX_MESSAGE_SIZE];
	int patrolflags;
	bot_waypoint_t *wp, *newwp, *newpatrolpoints;
	bot_match_t keyareamatch;
	bot_goal_t goal;

	newpatrolpoints = NULL;
	patrolflags = 0;
	//
	BotMatchVariable(match, KEYAREA, keyarea, MAX_MESSAGE_SIZE);
	//
	while(1) {
		if (!BotFindMatch(keyarea, &keyareamatch, MTCONTEXT_PATROLKEYAREA)) {
			EA_SayTeam(bs->playernum, "what do you say?");
			BotFreeWaypoints(newpatrolpoints);
			bs->patrolpoints = NULL;
			return qfalse;
		}
		BotMatchVariable(&keyareamatch, KEYAREA, keyarea, MAX_MESSAGE_SIZE);
		if (!BotGetMessageTeamGoal(bs, keyarea, &goal)) {
			//BotAI_BotInitialChat(bs, "cannotfind", keyarea, NULL);
			//BotEnterChat(bs->cs, 0, CHAT_TEAM);
			BotFreeWaypoints(newpatrolpoints);
			bs->patrolpoints = NULL;
			return qfalse;
		}
		//create a new waypoint
		newwp = BotCreateWayPoint(keyarea, goal.origin, goal.areanum);
		if (!newwp)
			break;
		//add the waypoint to the patrol points
		newwp->next = NULL;
		for (wp = newpatrolpoints; wp && wp->next; wp = wp->next);
		if (!wp) {
			newpatrolpoints = newwp;
			newwp->prev = NULL;
		}
		else {
			wp->next = newwp;
			newwp->prev = wp;
		}
		//
		if (keyareamatch.subtype & ST_BACK) {
			patrolflags = PATROL_LOOP;
			break;
		}
		else if (keyareamatch.subtype & ST_REVERSE) {
			patrolflags = PATROL_REVERSE;
			break;
		}
		else if (keyareamatch.subtype & ST_MORE) {
			BotMatchVariable(&keyareamatch, MORE, keyarea, MAX_MESSAGE_SIZE);
		}
		else {
			break;
		}
	}
	//
	if (!newpatrolpoints || !newpatrolpoints->next) {
		EA_SayTeam(bs->playernum, "I need more key points to patrol\n");
		BotFreeWaypoints(newpatrolpoints);
		newpatrolpoints = NULL;
		return qfalse;
	}
	//
	BotFreeWaypoints(bs->patrolpoints);
	bs->patrolpoints = newpatrolpoints;
	//
	bs->curpatrolpoint = bs->patrolpoints;
	bs->patrolflags = patrolflags;
	//
	return qtrue;
}

/*
==================
BotAddressedToBot
==================
*/
int BotAddressedToBot(bot_state_t *bs, bot_match_t *match) {
	char addressedto[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char name[MAX_MESSAGE_SIZE];
	char botname[128];
	int playerNum;
	bot_match_t addresseematch;

	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playerNum = PlayerOnSameTeamFromName(bs, netname);
	if (playerNum < 0) return qfalse;
	//if the message is addressed to someone
	if (match->subtype & ST_ADDRESSED) {
		BotMatchVariable(match, ADDRESSEE, addressedto, sizeof(addressedto));
		//the name of this bot
		PlayerName(bs->playernum, botname, 128);
		//
		while(BotFindMatch(addressedto, &addresseematch, MTCONTEXT_ADDRESSEE)) {
			if (addresseematch.type == MSG_EVERYONE) {
				return qtrue;
			}
			else if (addresseematch.type == MSG_MULTIPLENAMES) {
				BotMatchVariable(&addresseematch, TEAMMATE, name, sizeof(name));
				if (strlen(name)) {
					if (stristr(botname, name)) return qtrue;
					if (stristr(bs->subteam, name)) return qtrue;
				}
				BotMatchVariable(&addresseematch, MORE, addressedto, MAX_MESSAGE_SIZE);
			}
			else {
				BotMatchVariable(&addresseematch, TEAMMATE, name, MAX_MESSAGE_SIZE);
				if (strlen(name)) {
					if (stristr(botname, name)) return qtrue;
					if (stristr(bs->subteam, name)) return qtrue;
				}
				break;
			}
		}
		//Com_sprintf(buf, sizeof(buf), "not addressed to me but %s", addressedto);
		//EA_Say(bs->playernum, buf);
		return qfalse;
	}
	else {
		bot_match_t tellmatch;

		tellmatch.type = 0;
		//if this message wasn't directed solely to this bot
		if (!BotFindMatch(match->string, &tellmatch, MTCONTEXT_REPLYCHAT) ||
				tellmatch.type != MSG_CHATTELL) {
			//make sure not everyone reacts to this message
			if (random() > (float ) 1.0 / (NumPlayersOnSameTeam(bs)-1)) return qfalse;
		}
	}
	return qtrue;
}

/*
==================
BotGPSToPosition
==================
*/
int BotGPSToPosition(char *buf, vec3_t position) {
	int i, j = 0;
	int num, sign;

	for (i = 0; i < 3; i++) {
		num = 0;
		while(buf[j] == ' ') j++;
		if (buf[j] == '-') {
			j++;
			sign = -1;
		}
		else {
			sign = 1;
		}
		while (buf[j]) {
			if (buf[j] >= '0' && buf[j] <= '9') {
				num = num * 10 + buf[j] - '0';
				j++;
			}
			else {
				j++;
				break;
			}
		}
		BotAI_Print(PRT_MESSAGE, "%d\n", sign * num);
		position[i] = (float) sign * num;
	}
	return qtrue;
}

/*
==================
BotMatch_HelpAccompany
==================
*/
void BotMatch_HelpAccompany(bot_state_t *bs, bot_match_t *match) {
	int playernum, other, areanum;
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char itemname[MAX_MESSAGE_SIZE];
	bot_match_t teammatematch;
	aas_entityinfo_t entinfo;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//get the team mate name
	BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
	//get the player to help
	if (BotFindMatch(teammate, &teammatematch, MTCONTEXT_TEAMMATE) &&
			//if someone asks for him or herself
			teammatematch.type == MSG_ME) {
		//get the netname
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = PlayerFromName(netname);
		other = qfalse;
	}
	else {
		//asked for someone else
		playernum = FindPlayerByName(teammate);
		//if this is the bot self
		if (playernum == bs->playernum) {
			other = qfalse;
		}
		else if (!BotSameTeam(bs, playernum)) {
			//FIXME: say "I don't help the enemy"
			return;
		}
		else {
			other = qtrue;
		}
	}
	//if the bot doesn't know who to help (FindPlayerByName returned -1)
	if (playernum < 0) {
		if (other) BotAI_BotInitialChat(bs, "whois", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whois", netname, NULL);
		playernum = PlayerFromName(netname);
		BotEnterChat(bs->cs, playernum, CHAT_TELL);
		return;
	}
	//don't help or accompany yourself
	if (playernum == bs->playernum) {
		return;
	}
	//
	bs->teamgoal.entitynum = -1;
	BotEntityInfo(playernum, &entinfo);
	//if info is valid (in PVS)
	if (entinfo.valid) {
		areanum = BotPointAreaNum(entinfo.origin);
		if (areanum) {// && trap_AAS_AreaReachability(areanum)) {
			bs->teamgoal.entitynum = playernum;
			bs->teamgoal.areanum = areanum;
			VectorCopy(entinfo.origin, bs->teamgoal.origin);
			VectorSet(bs->teamgoal.mins, -8, -8, -8);
			VectorSet(bs->teamgoal.maxs, 8, 8, 8);
		}
	}
	//if no teamgoal yet
	if (bs->teamgoal.entitynum < 0) {
		//if near an item
		if (match->subtype & ST_NEARITEM) {
			//get the match variable
			BotMatchVariable(match, ITEM, itemname, sizeof(itemname));
			//
			if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
				//BotAI_BotInitialChat(bs, "cannotfind", itemname, NULL);
				//BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
				return;
			}
		}
	}
	//
	if (bs->teamgoal.entitynum < 0) {
		if (other) BotAI_BotInitialChat(bs, "whereis", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
		playernum = PlayerFromName(netname);
		BotEnterChat(bs->cs, playernum, CHAT_TEAM);
		return;
	}
	//the team mate
	bs->teammate = playernum;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = PlayerFromName(netname);
	//the team mate who ordered
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//last time the team mate was assumed visible
	bs->teammatevisible_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//get the team goal time
	bs->teamgoal_time = BotGetTime(match);
	//set the ltg type
	if (match->type == MSG_HELP) {
		bs->ltgtype = LTG_TEAMHELP;
		if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_HELP_TIME;
	}
	else {
		bs->ltgtype = LTG_TEAMACCOMPANY;
		if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
		bs->formation_dist = 128;
		bs->arrive_time = 0;
		//
		BotSetTeamStatus(bs);
		// remember last ordered task
		BotRememberLastOrderedTask(bs);
	}
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_DefendKeyArea
==================
*/
void BotMatch_DefendKeyArea(bot_state_t *bs, bot_match_t *match) {
	char itemname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//get the match variable
	BotMatchVariable(match, KEYAREA, itemname, sizeof(itemname));
	//
	if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		//BotAI_BotInitialChat(bs, "cannotfind", itemname, NULL);
		//BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
		return;
	}
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = PlayerFromName(netname);
	//the team mate who ordered
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_DEFENDKEYAREA;
	//get the team goal time
	bs->teamgoal_time = BotGetTime(match);
	//set the team goal time
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
	//away from defending
	bs->defendaway_time = 0;
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_GetItem
==================
*/
void BotMatch_GetItem(bot_state_t *bs, bot_match_t *match) {
	char itemname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int playerNum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//get the match variable
	BotMatchVariable(match, ITEM, itemname, sizeof(itemname));
	//
	if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		//BotAI_BotInitialChat(bs, "cannotfind", itemname, NULL);
		//BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
		return;
	}
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playerNum = PlayerOnSameTeamFromName(bs, netname);
	//
	bs->decisionmaker = playerNum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_GETITEM;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + TEAM_GETITEM_TIME;
	//
	BotSetTeamStatus(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_Camp
==================
*/
void BotMatch_Camp(bot_state_t *bs, bot_match_t *match) {
	int playernum, areanum;
	char netname[MAX_MESSAGE_SIZE];
	char itemname[MAX_MESSAGE_SIZE];
	aas_entityinfo_t entinfo;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//asked for someone else
	playernum = FindPlayerByName(netname);
	//if there's no valid player with this name
	if (playernum < 0) {
		BotAI_BotInitialChat(bs, "whois", netname, NULL);
		BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
		return;
	}
	//get the match variable
	BotMatchVariable(match, KEYAREA, itemname, sizeof(itemname));
	//in CTF it could be the base
	if (match->subtype & ST_THERE) {
		//camp at the spot the bot is currently standing
		bs->teamgoal.entitynum = bs->entitynum;
		bs->teamgoal.areanum = bs->areanum;
		VectorCopy(bs->origin, bs->teamgoal.origin);
		VectorSet(bs->teamgoal.mins, -8, -8, -8);
		VectorSet(bs->teamgoal.maxs, 8, 8, 8);
	}
	else if (match->subtype & ST_HERE) {
		//if this is the bot self
		if (playernum == bs->playernum) return;
		//
		bs->teamgoal.entitynum = -1;
		BotEntityInfo(playernum, &entinfo);
		//if info is valid (in PVS)
		if (entinfo.valid) {
			areanum = BotPointAreaNum(entinfo.origin);
			if (areanum) {// && trap_AAS_AreaReachability(areanum)) {
				//NOTE: just assume the bot knows where the person is
				//if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, playernum)) {
					bs->teamgoal.entitynum = playernum;
					bs->teamgoal.areanum = areanum;
					VectorCopy(entinfo.origin, bs->teamgoal.origin);
					VectorSet(bs->teamgoal.mins, -8, -8, -8);
					VectorSet(bs->teamgoal.maxs, 8, 8, 8);
				//}
			}
		}
		//if the other is not visible
		if (bs->teamgoal.entitynum < 0) {
			BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
			playernum = PlayerFromName(netname);
			BotEnterChat(bs->cs, playernum, CHAT_TELL);
			return;
		}
	}
	else if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		//BotAI_BotInitialChat(bs, "cannotfind", itemname, NULL);
		//playernum = PlayerFromName(netname);
		//BotEnterChat(bs->cs, playernum, CHAT_TELL);
		return;
	}
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_CAMPORDER;
	//get the team goal time
	bs->teamgoal_time = BotGetTime(match);
	//set the team goal time
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_CAMP_TIME;
	//not arrived yet
	bs->arrive_time = 0;
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_Patrol
==================
*/
void BotMatch_Patrol(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//get the patrol waypoints
	if (!BotGetPatrolWaypoints(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_PATROL;
	//get the team goal time
	bs->teamgoal_time = BotGetTime(match);
	//set the team goal time if not set already
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_PATROL_TIME;
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_GetFlag
==================
*/
void BotMatch_GetFlag(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (gametype == GT_CTF) {
		if (!ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (!ctf_neutralflag.areanum || !ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#endif
	else {
		return;
	}
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_GETFLAG;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
	// get an alternate route in ctf
	if (gametype == GT_CTF) {
		//get an alternative route goal towards the enemy base
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
	}
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_AttackEnemyBase
==================
*/
void BotMatch_AttackEnemyBase(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (gametype == GT_CTF) {
		BotMatch_GetFlag(bs, match);
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF || gametype == GT_OBELISK
#ifdef MISSIONPACK_HARVESTER
	|| gametype == GT_HARVESTER
#endif
	) {
		if (!redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
#endif
	else {
		return;
	}
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_ATTACKENEMYBASE;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
	bs->attackaway_time = 0;
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}

#ifdef MISSIONPACK_HARVESTER
/*
==================
BotMatch_Harvest
==================
*/
void BotMatch_Harvest(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (gametype == GT_HARVESTER) {
		if (!neutralobelisk.areanum || !redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
	else {
		return;
	}
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_HARVEST;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
	bs->harvestaway_time = 0;
	//
	BotSetTeamStatus(bs);
	// remember last ordered task
	BotRememberLastOrderedTask(bs);
	BotPrintTeamGoal(bs);
}
#endif

/*
==================
BotMatch_RushBase
==================
*/
void BotMatch_RushBase(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (gametype == GT_CTF) {
		if (!ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF
#ifdef MISSIONPACK_HARVESTER
	|| gametype == GT_HARVESTER
#endif
	) {
		if (!redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
#endif
	else {
		return;
	}
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_RUSHBASE;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
	bs->rushbaseaway_time = 0;
	//
	BotSetTeamStatus(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_TaskPreference
==================
*/
void BotMatch_TaskPreference(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_NETNAME];
	char teammatename[MAX_MESSAGE_SIZE];
	int teammate, preference;

	PlayerName(bs->playernum, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) != 0) return;

	BotMatchVariable(match, NETNAME, teammatename, sizeof(teammatename));
	teammate = PlayerFromName(teammatename);
	if (teammate < 0) return;

	preference = BotGetTeamMateTaskPreference(bs, teammate);
	switch(match->subtype)
	{
		case ST_DEFENDER:
		{
			preference &= ~TEAMTP_ATTACKER;
			preference |= TEAMTP_DEFENDER;
			break;
		}
		case ST_ATTACKER:
		{
			preference &= ~TEAMTP_DEFENDER;
			preference |= TEAMTP_ATTACKER;
			break;
		}
		case ST_ROAMER:
		{
			preference &= ~(TEAMTP_ATTACKER|TEAMTP_DEFENDER);
			break;
		}
	}
	BotSetTeamMateTaskPreference(bs, teammate, preference);
	//
	EasyPlayerName(teammate, teammatename, sizeof(teammatename));
	BotAI_BotInitialChat(bs, "keepinmind", teammatename, NULL);
	BotEnterChat(bs->cs, teammate, CHAT_TELL);
	BotVoiceChatOnly(bs, teammate, VOICECHAT_YES);
	EA_Action(bs->playernum, ACTION_AFFIRMATIVE);
}

/*
==================
BotMatch_ReturnFlag
==================
*/
void BotMatch_ReturnFlag(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	//if not in CTF mode
	if (
		gametype != GT_CTF
#ifdef MISSIONPACK
		&& gametype != GT_1FCTF
#endif
		)
		return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match))
		return;
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	//
	playernum = FindPlayerByName(netname);
	//
	bs->decisionmaker = playernum;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_RETURNFLAG;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + CTF_RETURNFLAG_TIME;
	bs->rushbaseaway_time = 0;
	//
	BotSetTeamStatus(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_JoinSubteam
==================
*/
void BotMatch_JoinSubteam(bot_state_t *bs, bot_match_t *match) {
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//get the sub team name
	BotMatchVariable(match, TEAMNAME, teammate, sizeof(teammate));
	//set the sub team name
	strncpy(bs->subteam, teammate, 32);
	bs->subteam[31] = '\0';
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	BotAI_BotInitialChat(bs, "joinedteam", teammate, NULL);
	playernum = PlayerFromName(netname);
	BotEnterChat(bs->cs, playernum, CHAT_TELL);
}

/*
==================
BotMatch_LeaveSubteam
==================
*/
void BotMatch_LeaveSubteam(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	if (strlen(bs->subteam))
	{
		BotAI_BotInitialChat(bs, "leftteam", bs->subteam, NULL);
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = PlayerFromName(netname);
		BotEnterChat(bs->cs, playernum, CHAT_TELL);
	} //end if
	strcpy(bs->subteam, "");
}

/*
==================
BotMatch_LeaveSubteam
==================
*/
void BotMatch_WhichTeam(bot_state_t *bs, bot_match_t *match) {
	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	if (strlen(bs->subteam)) {
		BotAI_BotInitialChat(bs, "inteam", bs->subteam, NULL);
	}
	else {
		BotAI_BotInitialChat(bs, "noteam", NULL);
	}
	BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
}

/*
==================
BotMatch_CheckPoint
==================
*/
void BotMatch_CheckPoint(bot_state_t *bs, bot_match_t *match) {
	int areanum, playernum;
	char buf[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	vec3_t position;
	bot_waypoint_t *cp;

	if (!TeamPlayIsOn()) return;
	//
	BotMatchVariable(match, POSITION, buf, MAX_MESSAGE_SIZE);
	VectorClear(position);
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = PlayerFromName(netname);
	//BotGPSToPosition(buf, position);
	sscanf(buf, "%f %f %f", &position[0], &position[1], &position[2]);
	position[2] += 0.5;
	areanum = BotPointAreaNum(position);
	if (!areanum) {
		if (BotAddressedToBot(bs, match)) {
			BotAI_BotInitialChat(bs, "checkpoint_invalid", NULL);
			BotEnterChat(bs->cs, playernum, CHAT_TELL);
		}
		return;
	}
	//
	BotMatchVariable(match, NAME, buf, MAX_MESSAGE_SIZE);
	//check if there already exists a checkpoint with this name
	cp = BotFindWayPoint(bs->checkpoints, buf);
	if (cp) {
		if (cp->next) cp->next->prev = cp->prev;
		if (cp->prev) cp->prev->next = cp->next;
		else bs->checkpoints = cp->next;
		cp->inuse = qfalse;
	}
	//create a new check point
	cp = BotCreateWayPoint(buf, position, areanum);
	//add the check point to the bot's known chech points
	cp->next = bs->checkpoints;
	if (bs->checkpoints) bs->checkpoints->prev = cp;
	bs->checkpoints = cp;
	//
	if (BotAddressedToBot(bs, match)) {
		Com_sprintf(buf, sizeof(buf), "%1.0f %1.0f %1.0f", cp->goal.origin[0],
													cp->goal.origin[1],
													cp->goal.origin[2]);

		BotAI_BotInitialChat(bs, "checkpoint_confirm", cp->name, buf, NULL);
		BotEnterChat(bs->cs, playernum, CHAT_TELL);
	}
}

/*
==================
BotMatch_FormationSpace
==================
*/
void BotMatch_FormationSpace(bot_state_t *bs, bot_match_t *match) {
	char buf[MAX_MESSAGE_SIZE];
	float space;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	BotMatchVariable(match, NUMBER, buf, MAX_MESSAGE_SIZE);
	//if it's the distance in feet
	if (match->subtype & ST_FEET) space = 0.3048 * 32 * atof(buf);
	//else it's in meters
	else space = 32 * atof(buf);
	//check if the formation intervening space is valid
	if (space < 48 || space > 500) space = 128;
	bs->formation_dist = space;
}

/*
==================
BotMatch_Dismiss
==================
*/
void BotMatch_Dismiss(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = PlayerFromName(netname);
	//
	bs->decisionmaker = playernum;
	//
	bs->ltgtype = 0;
	bs->lead_time = 0;
	bs->lastgoal_ltgtype = 0;
	//
	BotAI_BotInitialChat(bs, "dismissed", NULL);
	BotEnterChat(bs->cs, playernum, CHAT_TELL);
}

/*
==================
BotMatch_Suicide
==================
*/
void BotMatch_Suicide(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	EA_Command(bs->playernum, "kill");
	//
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = PlayerFromName(netname);
	//
	BotVoiceChat(bs, playernum, VOICECHAT_TAUNT);
	EA_Action(bs->playernum, ACTION_AFFIRMATIVE);
}

/*
==================
BotMatch_StartTeamLeaderShip
==================
*/
void BotMatch_StartTeamLeaderShip(bot_state_t *bs, bot_match_t *match) {
	int playernum;
	char teammate[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;
	//if chats for him or herself
	if (match->subtype & ST_I) {
		//get the team mate that will be the team leader
		BotMatchVariable(match, NETNAME, teammate, sizeof(teammate));
		strncpy(bs->teamleader, teammate, sizeof(bs->teamleader));
		bs->teamleader[sizeof(bs->teamleader)-1] = '\0';
	}
	//chats for someone else
	else {
		//get the team mate that will be the team leader
		BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
		playernum = FindPlayerByName(teammate);
		if (playernum >= 0) PlayerName(playernum, bs->teamleader, sizeof(bs->teamleader));
	}
}

/*
==================
BotMatch_StopTeamLeaderShip
==================
*/
void BotMatch_StopTeamLeaderShip(bot_state_t *bs, bot_match_t *match) {
	int playernum;
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;
	//get the team mate that stops being the team leader
	BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
	//if chats for him or herself
	if (match->subtype & ST_I) {
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = FindPlayerByName(netname);
	}
	//chats for someone else
	else {
		playernum = FindPlayerByName(teammate);
	} //end else
	if (playernum >= 0) {
		if (!Q_stricmp(bs->teamleader, PlayerName(playernum, netname, sizeof(netname)))) {
			bs->teamleader[0] = '\0';
			notleader[playernum] = qtrue;
		}
	}
}

/*
==================
BotMatch_WhoIsTeamLeader
==================
*/
void BotMatch_WhoIsTeamLeader(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;

	PlayerName(bs->playernum, netname, sizeof(netname));
	//if this bot IS the team leader
	if (!Q_stricmp(netname, bs->teamleader)) {
		EA_SayTeam(bs->playernum, "I'm the team leader\n");
	}
}

/*
==================
BotMatch_WhatAreYouDoing
==================
*/
void BotMatch_WhatAreYouDoing(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	char goalname[MAX_MESSAGE_SIZE];
	int playernum;

	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//
	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyPlayerName(bs->teammate, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "helping", netname, NULL);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyPlayerName(bs->teammate, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "accompanying", netname, NULL);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_BotInitialChat(bs, "defending", goalname, NULL);
			break;
		}
		case LTG_GETITEM:
		{
			BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_BotInitialChat(bs, "gettingitem", goalname, NULL);
			break;
		}
		case LTG_KILL:
		{
			PlayerName(bs->teamgoal.entitynum, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "killing", netname, NULL);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_BotInitialChat(bs, "camping", NULL);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_BotInitialChat(bs, "patrolling", NULL);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_BotInitialChat(bs, "capturingflag", NULL);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_BotInitialChat(bs, "rushingbase", NULL);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_BotInitialChat(bs, "returningflag", NULL);
			break;
		}
#ifdef MISSIONPACK
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_BotInitialChat(bs, "attackingenemybase", NULL);
			break;
		}
#ifdef MISSIONPACK_HARVESTER
		case LTG_HARVEST:
		{
			BotAI_BotInitialChat(bs, "harvesting", NULL);
			break;
		}
#endif
#endif
		default:
		{
			BotAI_BotInitialChat(bs, "roaming", NULL);
			break;
		}
	}
	//chat what the bot is doing
	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = PlayerFromName(netname);
	BotEnterChat(bs->cs, playernum, CHAT_TELL);
}

/*
==================
BotMatch_WhatIsMyCommand
==================
*/
void BotMatch_WhatIsMyCommand(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_NETNAME];

	PlayerName(bs->playernum, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) != 0) return;
	bs->forceorders = qtrue;
}

/*
==================
BotNearestVisibleItem
==================
*/
float BotNearestVisibleItem(bot_state_t *bs, char *itemname, bot_goal_t *goal) {
	int i;
	char name[64];
	bot_goal_t tmpgoal;
	float dist, bestdist;
	vec3_t dir;
	bsp_trace_t trace;

	bestdist = 999999;
	i = -1;
	do {
		i = BotGetLevelItemGoal(i, itemname, &tmpgoal);
		BotGoalName(tmpgoal.number, name, sizeof(name));
		if (Q_stricmp(itemname, name) != 0)
			continue;
		VectorSubtract(tmpgoal.origin, bs->origin, dir);
		dist = VectorLength(dir);
		if (dist < bestdist) {
			//trace from start to end
			BotAI_Trace(&trace, bs->eye, NULL, NULL, tmpgoal.origin, bs->playernum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
			if (trace.fraction >= 1.0) {
				bestdist = dist;
				memcpy(goal, &tmpgoal, sizeof(bot_goal_t));
			}
		}
	} while(i > 0);
	return bestdist;
}

/*
==================
BotMatch_WhereAreYou
==================
*/
void BotMatch_WhereAreYou(bot_state_t *bs, bot_match_t *match) {
	float dist, bestdist;
	int i, redtt, bluett, playernum;
	char *teamlocation;
	char *bestitemname;
	bot_goal_t goal;
	gitem_t *it;
	char netname[MAX_MESSAGE_SIZE];
	char *nearbyitems[] = {
#ifdef MISSIONPACK
		"Red Obelisk",
		"Blue Obelisk",
#ifdef MISSIONPACK_HARVESTER
		"Neutral Obelisk",
#endif
#endif
		NULL
	};
	//
	if (!TeamPlayIsOn())
		return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match))
		return;

	bestitemname = NULL;
	bestdist = 999999;
	for (i = 1; i < BG_NumItems(); i++) {
		it = BG_ItemForItemNum( i );

		if ( !it->classname || !*it->classname ) {
			continue;
		}

		//ignore health, ammo, holdables, small armor, and Red Cube and Blue Cube
		if ( it->giType == IT_HEALTH
			|| it->giType == IT_AMMO
			|| it->giType == IT_HOLDABLE
#ifndef TURTLEARENA // NOARMOR
			|| ( it->giType == IT_ARMOR && it->quantity < 50 )
#endif
#ifdef MISSIONPACK
			|| ( it->giType == IT_TEAM && it->giTag == 0 )
#endif
			) {
			continue;
		}

		dist = BotNearestVisibleItem(bs, it->pickup_name, &goal);
		if (dist < bestdist) {
			bestdist = dist;
			bestitemname = it->pickup_name;
		}
	}
	for (i = 0; nearbyitems[i]; i++) {
		dist = BotNearestVisibleItem(bs, nearbyitems[i], &goal);
		if (dist < bestdist) {
			bestdist = dist;
			bestitemname = nearbyitems[i];
		}
	}
	if (bestitemname) {
		if (gametype == GT_CTF
#ifdef MISSIONPACK
			|| gametype == GT_1FCTF
			|| gametype == GT_OBELISK
#ifdef MISSIONPACK_HARVESTER
			|| gametype == GT_HARVESTER
#endif
#endif
			) {
#ifdef MISSIONPACK
			if (gametype == GT_OBELISK
#ifdef MISSIONPACK_HARVESTER
				|| gametype == GT_HARVESTER
#endif
				) {
				redtt = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, redobelisk.areanum, TFL_DEFAULT);
				bluett = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, blueobelisk.areanum, TFL_DEFAULT);
			}
			else
#endif
			{
				redtt = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, ctf_redflag.areanum, TFL_DEFAULT);
				bluett = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, ctf_blueflag.areanum, TFL_DEFAULT);
			}

			// unpatched q3 used 'ctflocation', some games still use it
			if ( BotNumInitialChats( bs->cs, "teamlocation" ) ) {
				teamlocation = "teamlocation";
			} else {
				teamlocation = "ctflocation";
			}

			if (redtt < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, teamlocation, bestitemname, "red", NULL);
			}
			else if (bluett < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, teamlocation, bestitemname, "blue", NULL);
			}
			else {
				BotAI_BotInitialChat(bs, "location", bestitemname, NULL);
			}
		}
		else {
			BotAI_BotInitialChat(bs, "location", bestitemname, NULL);
		}
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = PlayerFromName(netname);
		BotEnterChat(bs->cs, playernum, CHAT_TELL);
	}
}

/*
==================
BotMatch_LeadTheWay
==================
*/
void BotMatch_LeadTheWay(bot_state_t *bs, bot_match_t *match) {
	aas_entityinfo_t entinfo;
	char netname[MAX_MESSAGE_SIZE], teammate[MAX_MESSAGE_SIZE];
	int playernum, areanum, other;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;
	//if someone asks for someone else
	if (match->subtype & ST_SOMEONE) {
		//get the team mate name
		BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
		playernum = FindPlayerByName(teammate);
		//if this is the bot self
		if (playernum == bs->playernum) {
			other = qfalse;
		}
		else if (!BotSameTeam(bs, playernum)) {
			//FIXME: say "I don't help the enemy"
			return;
		}
		else {
			other = qtrue;
		}
	}
	else {
		//get the netname
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = PlayerFromName(netname);
		other = qfalse;
	}
	//if the bot doesn't know who to help (FindPlayerByName returned -1)
	if (playernum < 0) {
		BotAI_BotInitialChat(bs, "whois", netname, NULL);
		BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
		return;
	}
	//
	bs->lead_teamgoal.entitynum = -1;
	BotEntityInfo(playernum, &entinfo);
	//if info is valid (in PVS)
	if (entinfo.valid) {
		areanum = BotPointAreaNum(entinfo.origin);
		if (areanum) { // && trap_AAS_AreaReachability(areanum)) {
			bs->lead_teamgoal.entitynum = playernum;
			bs->lead_teamgoal.areanum = areanum;
			VectorCopy(entinfo.origin, bs->lead_teamgoal.origin);
			VectorSet(bs->lead_teamgoal.mins, -8, -8, -8);
			VectorSet(bs->lead_teamgoal.maxs, 8, 8, 8);
		}
	}

	if (bs->teamgoal.entitynum < 0) {
		if (other) BotAI_BotInitialChat(bs, "whereis", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
		BotEnterChat(bs->cs, bs->playernum, CHAT_TEAM);
		return;
	}
	bs->lead_teammate = playernum;
	bs->lead_time = FloatTime() + TEAM_LEAD_TIME;
	bs->leadvisible_time = 0;
	bs->leadmessage_time = -(FloatTime() + 2 * random());
}

/*
==================
BotMatch_Kill
==================
*/
void BotMatch_Kill(bot_state_t *bs, bot_match_t *match) {
	char enemy[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int playernum;

	if (!TeamPlayIsOn()) return;
	//if not addressed to this bot
	if (!BotAddressedToBot(bs, match)) return;

	BotMatchVariable(match, ENEMY, enemy, sizeof(enemy));
	//
	playernum = FindEnemyByName(bs, enemy);
	if (playernum < 0) {
		BotAI_BotInitialChat(bs, "whois", enemy, NULL);
		BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		playernum = PlayerFromName(netname);
		BotEnterChat(bs->cs, playernum, CHAT_TELL);
		return;
	}
	bs->teamgoal.entitynum = playernum;
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//set the ltg type
	bs->ltgtype = LTG_KILL;
	//set the team goal time
	bs->teamgoal_time = FloatTime() + TEAM_KILL_SOMEONE;
	//
	BotSetTeamStatus(bs);
	BotPrintTeamGoal(bs);
}

/*
==================
BotMatch_CTF
==================
*/
void BotMatch_CTF(bot_state_t *bs, bot_match_t *match) {

	char flag[128], netname[MAX_NETNAME];

	if (gametype == GT_CTF) {
		BotMatchVariable(match, FLAG, flag, sizeof(flag));
		if (match->subtype & ST_GOTFLAG) {
			if (!Q_stricmp(flag, "red")) {
				bs->redflagstatus = 1;
				if (BotTeam(bs) == TEAM_BLUE) {
					BotMatchVariable(match, NETNAME, netname, sizeof(netname));
					bs->flagcarrier = PlayerFromName(netname);
				}
			}
			else {
				bs->blueflagstatus = 1;
				if (BotTeam(bs) == TEAM_RED) {
					BotMatchVariable(match, NETNAME, netname, sizeof(netname));
					bs->flagcarrier = PlayerFromName(netname);
				}
			}
			bs->flagstatuschanged = 1;
			bs->lastflagcapture_time = FloatTime();
		}
		else if (match->subtype & ST_CAPTUREDFLAG) {
			bs->redflagstatus = 0;
			bs->blueflagstatus = 0;
			bs->flagcarrier = 0;
			bs->flagstatuschanged = 1;
		}
		else if (match->subtype & ST_RETURNEDFLAG) {
			if (!Q_stricmp(flag, "red")) bs->redflagstatus = 0;
			else bs->blueflagstatus = 0;
			bs->flagstatuschanged = 1;
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (match->subtype & ST_1FCTFGOTFLAG) {
			BotMatchVariable(match, NETNAME, netname, sizeof(netname));
			bs->flagcarrier = PlayerFromName(netname);
			bs->lastflagcapture_time = FloatTime();
		}
	}
#endif
}

void BotMatch_EnterGame(bot_state_t *bs, bot_match_t *match) {
	int playernum;
	char netname[MAX_NETNAME];
	//char buf[MAX_SAY_TEXT];

	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = FindPlayerByName(netname);
	if (playernum >= 0) {
		notleader[playernum] = qfalse;
	}
	//NOTE: eliza chats will catch this
	//if (playernum != bs->playernum) {
	//	Com_sprintf(buf, sizeof(buf), "heya %s", netname);
	//	EA_Say(bs->playernum, buf);
	//}
}

void BotMatch_NewLeader(bot_state_t *bs, bot_match_t *match) {
	int playernum;
	char netname[MAX_NETNAME];

	BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	playernum = FindPlayerByName(netname);
	if (!BotSameTeam(bs, playernum))
		return;
	Q_strncpyz(bs->teamleader, netname, sizeof(bs->teamleader));
}

/*
==================
BotMatchMessage
==================
*/
int BotMatchMessage(bot_state_t *bs, char *message) {
	bot_match_t match;

	match.type = 0;
	//if it is an unknown message
	if (!BotFindMatch(message, &match, MTCONTEXT_MISC
											|MTCONTEXT_INITIALTEAMCHAT
											|MTCONTEXT_CTF)) {
		return qfalse;
	}
	//react to the found message
	switch(match.type)
	{
		case MSG_HELP:					//someone calling for help
		case MSG_ACCOMPANY:				//someone calling for company
		{
			BotMatch_HelpAccompany(bs, &match);
			break;
		}
		case MSG_DEFENDKEYAREA:			//teamplay defend a key area
		{
			BotMatch_DefendKeyArea(bs, &match);
			break;
		}
		case MSG_CAMP:					//camp somewhere
		{
			BotMatch_Camp(bs, &match);
			break;
		}
		case MSG_PATROL:				//patrol between several key areas
		{
			BotMatch_Patrol(bs, &match);
			break;
		}
		//CTF & 1FCTF
		case MSG_GETFLAG:				//ctf get the enemy flag
		{
			BotMatch_GetFlag(bs, &match);
			break;
		}
#ifdef MISSIONPACK
		//CTF & 1FCTF & Obelisk & Harvester
		case MSG_ATTACKENEMYBASE:
		{
			BotMatch_AttackEnemyBase(bs, &match);
			break;
		}
#ifdef MISSIONPACK_HARVESTER
		//Harvester
		case MSG_HARVEST:
		{
			BotMatch_Harvest(bs, &match);
			break;
		}
#endif
#endif
		//CTF & 1FCTF & Harvester
		case MSG_RUSHBASE:				//ctf rush to the base
		{
			BotMatch_RushBase(bs, &match);
			break;
		}
		//CTF & 1FCTF
		case MSG_RETURNFLAG:
		{
			BotMatch_ReturnFlag(bs, &match);
			break;
		}
		//CTF & 1FCTF & Obelisk & Harvester
		case MSG_TASKPREFERENCE:
		{
			BotMatch_TaskPreference(bs, &match);
			break;
		}
		//CTF & 1FCTF
		case MSG_CTF:
		{
			BotMatch_CTF(bs, &match);
			break;
		}
		case MSG_GETITEM:
		{
			BotMatch_GetItem(bs, &match);
			break;
		}
		case MSG_JOINSUBTEAM:			//join a sub team
		{
			BotMatch_JoinSubteam(bs, &match);
			break;
		}
		case MSG_LEAVESUBTEAM:			//leave a sub team
		{
			BotMatch_LeaveSubteam(bs, &match);
			break;
		}
		case MSG_WHICHTEAM:
		{
			BotMatch_WhichTeam(bs, &match);
			break;
		}
		case MSG_CHECKPOINT:			//remember a check point
		{
			BotMatch_CheckPoint(bs, &match);
			break;
		}
		case MSG_CREATENEWFORMATION:	//start the creation of a new formation
		{
			EA_SayTeam(bs->playernum, "the part of my brain to create formations has been damaged");
			break;
		}
		case MSG_FORMATIONPOSITION:		//tell someone his/her position in the formation
		{
			EA_SayTeam(bs->playernum, "the part of my brain to create formations has been damaged");
			break;
		}
		case MSG_FORMATIONSPACE:		//set the formation space
		{
			BotMatch_FormationSpace(bs, &match);
			break;
		}
		case MSG_DOFORMATION:			//form a certain formation
		{
			break;
		}
		case MSG_DISMISS:				//dismiss someone
		{
			BotMatch_Dismiss(bs, &match);
			break;
		}
		case MSG_STARTTEAMLEADERSHIP:	//someone will become the team leader
		{
			BotMatch_StartTeamLeaderShip(bs, &match);
			break;
		}
		case MSG_STOPTEAMLEADERSHIP:	//someone will stop being the team leader
		{
			BotMatch_StopTeamLeaderShip(bs, &match);
			break;
		}
		case MSG_WHOISTEAMLAEDER:
		{
			BotMatch_WhoIsTeamLeader(bs, &match);
			break;
		}
		case MSG_WHATAREYOUDOING:		//ask a bot what he/she is doing
		{
			BotMatch_WhatAreYouDoing(bs, &match);
			break;
		}
		case MSG_WHATISMYCOMMAND:
		{
			BotMatch_WhatIsMyCommand(bs, &match);
			break;
		}
		case MSG_WHEREAREYOU:
		{
			BotMatch_WhereAreYou(bs, &match);
			break;
		}
		case MSG_LEADTHEWAY:
		{
			BotMatch_LeadTheWay(bs, &match);
			break;
		}
		case MSG_KILL:
		{
			BotMatch_Kill(bs, &match);
			break;
		}
		case MSG_ENTERGAME:				//someone entered the game
		{
			BotMatch_EnterGame(bs, &match);
			break;
		}
		case MSG_NEWLEADER:
		{
			BotMatch_NewLeader(bs, &match);
			break;
		}
		case MSG_WAIT:
		{
			break;
		}
		case MSG_SUICIDE:
		{
			BotMatch_Suicide(bs, &match);
			break;
		}
		default:
		{
			BotAI_Print(PRT_MESSAGE, "unknown match type\n");
			break;
		}
	}
	return qtrue;
}
