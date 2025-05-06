/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "LfgActions.h"

#include "AiFactory.h"
#include "ItemVisitors.h"
#include "LFGMgr.h"
#include "LFGPackets.h"
#include "Opcodes.h"
#include "Playerbots.h"
#include "World.h"
#include "WorldPacket.h"

using namespace lfg;


bool LfgJoinAction::Execute(Event event) { return JoinLFG(); }

uint32 LfgJoinAction::GetRoles()
{
    if (!sRandomPlayerbotMgr->IsRandomBot(bot))
    {
        if (botAI->IsTank(bot))
            return PLAYER_ROLE_TANK;
        if (botAI->IsHeal(bot))
            return PLAYER_ROLE_HEALER;
        else
            return PLAYER_ROLE_DAMAGE;
    }

    uint8 spec = AiFactory::GetPlayerSpecTab(bot);
    switch (bot->getClass())
    {
        case CLASS_DRUID:
            if (spec == 2)
                return PLAYER_ROLE_HEALER;
            else if (spec == 1 && bot->HasAura(16931) /* thick hide */)
                return PLAYER_ROLE_TANK;
            else
                return PLAYER_ROLE_DAMAGE;
            break;
        case CLASS_PALADIN:
            if (spec == 1)
                return PLAYER_ROLE_TANK;
            else if (!spec)
                return PLAYER_ROLE_HEALER;
            else
                return PLAYER_ROLE_DAMAGE;
            break;
        case CLASS_PRIEST:
            if (spec != 2)
                return PLAYER_ROLE_HEALER;
            else
                return PLAYER_ROLE_DAMAGE;
            break;
        case CLASS_SHAMAN:
            if (spec == 2)
                return PLAYER_ROLE_HEALER;
            else
                return PLAYER_ROLE_DAMAGE;
            break;
        case CLASS_WARRIOR:
            if (spec == 2)
                return PLAYER_ROLE_TANK;
            else
                return PLAYER_ROLE_DAMAGE;
            break;
        case CLASS_DEATH_KNIGHT:
            if (spec == 0)
                return PLAYER_ROLE_TANK;
            else
                return PLAYER_ROLE_DAMAGE;
            break;

        default:
            return PLAYER_ROLE_DAMAGE;
			break;
    }

    return PLAYER_ROLE_DAMAGE;
}

bool LfgJoinAction::JoinLFG()
{
    // check if already in lfg
    LfgState state = sLFGMgr->GetState(bot->GetGUID());
    if (state != LFG_STATE_NONE)
        return false;

    /*ItemCountByQuality visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_EQUIP);
    bool random = urand(0, 100) < 20;
    bool heroic = urand(0, 100) < 50 &&
                  (visitor.count[ITEM_QUALITY_EPIC] >= 3 || visitor.count[ITEM_QUALITY_RARE] >= 10) &&
                  bot->GetLevel() >= 70;
    bool rbotAId = !heroic && (urand(0, 100) < 50 && visitor.count[ITEM_QUALITY_EPIC] >= 5 &&
                               (bot->GetLevel() == 60 || bot->GetLevel() == 70 || bot->GetLevel() == 80));*/

    LfgDungeonSet list;
    std::vector<uint32> selected;
    std::vector<uint32> dungeons = sRandomPlayerbotMgr->LfgDungeons[bot->GetTeamId()];
    if (!dungeons.size())
        return false;

    // Often skip dungeon browser and go right in to raid browser
    if (urand(0,99) < 40)
    {
        for (std::vector<uint32>::iterator i = dungeons.begin(); i != dungeons.end(); ++i)
        {
            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(*i);
            if (!dungeon || (dungeon->TypeID != LFG_TYPE_RANDOM && dungeon->TypeID != LFG_TYPE_DUNGEON &&
                            dungeon->TypeID != LFG_TYPE_HEROIC))
                continue;

            const auto& botLevel = bot->GetLevel();

            /*LFG_TYPE_RANDOM on classic is 15-58 so bot over level 25 will never queue*/
            if (dungeon->MinLevel && (botLevel < dungeon->MinLevel || botLevel > dungeon->MaxLevel) ||
                (botLevel > dungeon->MinLevel + 10 && dungeon->TypeID == LFG_TYPE_DUNGEON))
                continue;

            selected.push_back(dungeon->ID);
            list.insert(dungeon->ID);
        }
    }

    if (!selected.size())
    {
        for (std::vector<uint32>::iterator i = dungeons.begin(); i != dungeons.end(); ++i)
        {
            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(*i);
            if (!dungeon || dungeon->TypeID != LFG_TYPE_RAID)
                continue;

            const auto& botLevel = bot->GetLevel();
            if (dungeon->GroupID < 6 || dungeon->GroupID > 9)
                continue;

            if ((botLevel == 60 && dungeon->GroupID == 6) ||                        // Raid Classic
                (botLevel == 70 && dungeon->GroupID == 7) ||                        // Raid TBC
                (botLevel == 80 && (dungeon->GroupID == 8 || dungeon->GroupID == 9)))  // Raid WotLK
            {
                selected.push_back(dungeon->ID);
                list.insert(dungeon->ID);
            }
        }
    }

    if (!selected.size())
        return false;

    bool many = list.size() > 1;
    LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(*list.begin());

    // check role for console msg
    std::string _roles = "multiple roles";
    uint32 roleMask = GetRoles();
    if (roleMask & PLAYER_ROLE_TANK)
        _roles = "TANK";

    if (roleMask & PLAYER_ROLE_HEALER)
        _roles = "HEAL";

    if (roleMask & PLAYER_ROLE_DAMAGE)
        _roles = "DPS";

    LOG_INFO("playerbots", "Bot {} {}:{} <{}>: queues LFG, Dungeon as {} ({})", bot->GetGUID().ToString().c_str(),
             bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName().c_str(), _roles,
             many ? "several dungeons" : dungeon->Name[0]);

    std::string _gs = std::to_string(static_cast<int>(bot->GetAverageItemLevelForDF()));
    std::string comment = "Bot " + _roles + " GS:" + _gs + " for LFG";
    WorldPacket* data = new WorldPacket(CMSG_LFG_JOIN);
    *data << (uint32)roleMask;         // Roles
    *data << (bool)false;              // NoPartialClear
    *data << (bool)false;              // Achievements
    *data << (uint8)(list.size());     // Slots count
    for (uint32 dungeon : list)
        *data << (uint32)dungeon;     // Slot entries
    
    *data << (uint8)3                 // Needs array size (always 3)
        << (uint8)0
        << (uint8)0
        << (uint8)0;
    
    *data << comment;                 // Comment
    *data << _gs;                     // Gearscore or "gs"
    bot->GetSession()->QueuePacket(data);

    return true;
}

bool LfgRoleCheckAction::Execute(Event event)
{
    if (Group* group = bot->GetGroup())
    {
        uint32 currentRoles = sLFGMgr->GetRoles(bot->GetGUID());
        uint32 newRoles = GetRoles();
        // if (currentRoles == newRoles)
        //     return false;

        
        WorldPacket* packet = new WorldPacket(CMSG_LFG_SET_ROLES);
        *packet << (uint8)newRoles;
        bot->GetSession()->QueuePacket(packet);
        // sLFGMgr->SetRoles(bot->GetGUID(), newRoles);
        // sLFGMgr->UpdateRoleCheck(group->GetGUID(), bot->GetGUID(), newRoles);

        LOG_INFO("playerbots", "Bot {} {}:{} <{}>: LFG roles checked", bot->GetGUID().ToString().c_str(),
                 bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName().c_str());

        return true;
    }

    return false;
}

bool LfgAcceptAction::Execute(Event event)
{
    uint32 id = AI_VALUE(uint32, "lfg proposal");

    // Try accept if already stored
    if (id)
    {
        LOG_INFO("playerbots", "Bot {} {}:{} <{}> accepts LFG proposal {}", bot->GetGUID().ToString().c_str(),
                 bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName().c_str(), id);

        botAI->GetAiObjectContext()->GetValue<uint32>("lfg proposal")->Set(0);

        if (bot->IsInCombat() || bot->isDead())
        {
            WorldPacket* packet = new WorldPacket(CMSG_LFG_PROPOSAL_RESULT);
            *packet << id << false;
            bot->GetSession()->QueuePacket(packet);
            return true;
        }

        if (bot->IsInCombat())
        {
            bot->CombatStop(true);
            bot->ClearInCombat();
            bot->ClearUnitState(UNIT_STATE_ALL_STATE);
        }

        if (bot->isDead())
        {
            bot->ResurrectPlayer(0.25f);
            bot->SpawnCorpseBones();
        }

        WorldPacket* packet = new WorldPacket(CMSG_LFG_PROPOSAL_RESULT);
        *packet << id << true;
        bot->GetSession()->QueuePacket(packet);

        if (sRandomPlayerbotMgr->IsRandomBot(bot) && !bot->GetGroup())
        {
            sRandomPlayerbotMgr->Refresh(bot);
            botAI->ResetStrategies();
        }

        botAI->Reset();
        return true;
    }

    // If we get the proposal packet, accept immediately
    if (!event.getPacket().empty())
    {
        WorldPacket p(event.getPacket());
        uint32 dungeonId;
        uint8 state;
        p >> dungeonId >> state >> id;

        if (id)
        {
            if (bot->IsInCombat() || bot->isDead())
            {
                WorldPacket* packet = new WorldPacket(CMSG_LFG_PROPOSAL_RESULT);
                *packet << id << false;
                bot->GetSession()->QueuePacket(packet);
                return true;
            }
    
            if (bot->IsInCombat())
            {
                bot->CombatStop(true);
                bot->ClearInCombat();
                bot->ClearUnitState(UNIT_STATE_ALL_STATE);
            }
    
            if (bot->isDead())
            {
                bot->ResurrectPlayer(0.25f);
                bot->SpawnCorpseBones();
            }

            botAI->GetAiObjectContext()->GetValue<uint32>("lfg proposal")->Set(0);

            WorldPacket* packet = new WorldPacket(CMSG_LFG_PROPOSAL_RESULT);
            *packet << id << true;
            bot->GetSession()->QueuePacket(packet);

            if (sRandomPlayerbotMgr->IsRandomBot(bot) && !bot->GetGroup())
            {
                sRandomPlayerbotMgr->Refresh(bot);
                botAI->ResetStrategies();
            }

            botAI->Reset();
            return true;
        }
    }

    return false;
}


bool LfgLeaveAction::Execute(Event event)
{
    // Don't leave if lfg strategy enabled
    // if (botAI->HasStrategy("lfg", BOT_STATE_NON_COMBAT))
    //    return false;

    // Don't leave if already invited / in dungeon, but always leave in raidbrowser
    if (sLFGMgr->GetState(bot->GetGUID()) > LFG_STATE_QUEUED && sLFGMgr->GetState(bot->GetGUID()) != LFG_STATE_RAIDBROWSER)
        return false;

    WorldPacket* packet = new WorldPacket(CMSG_LFG_LEAVE);
    bot->GetSession()->QueuePacket(packet);
    // sLFGMgr->LeaveLfg(bot->GetGUID());
    return true;
}

bool LfgLeaveAction::isUseful() { return true; }

bool LfgTeleportAction::Execute(Event event)
{
    bool out = false;

    WorldPacket p(event.getPacket());
    if (!p.empty())
    {
        p.rpos(0);
        p >> out;
    }

    bot->ClearUnitState(UNIT_STATE_ALL_STATE);

    WorldPacket* packet = new WorldPacket(CMSG_LFG_TELEPORT);
    *packet << out;
    bot->GetSession()->QueuePacket(packet);
    // sLFGMgr->TeleportPlayer(bot, out);

    return true;
}

bool LfgJoinAction::isUseful()
{
    if (!sPlayerbotAIConfig->randomBotJoinLfg)
    {
        // botAI->ChangeStrategy("-lfg", BOT_STATE_NON_COMBAT);
        return false;
    }

    if (bot->GetLevel() < 15)
        return false;
	
    if (bot->GetAverageItemLevelForDF() < 10)
        return false;

	// don't use if active player master
    if (GET_PLAYERBOT_AI(bot)->IsRealPlayer())
        return false;

    if (bot->GetGroup() && bot->GetGroup()->GetLeaderGUID() != bot->GetGUID())
    {
        // botAI->ChangeStrategy("-lfg", BOT_STATE_NON_COMBAT);
        return false;
    }

    if (bot->IsBeingTeleported())
        return false;

    if (bot->InBattleground())
        return false;

    if (bot->InBattlegroundQueue())
        return false;

    if (bot->isDead())
        return false;

    if (!sRandomPlayerbotMgr->IsRandomBot(bot))
        return false;

    Map* map = bot->GetMap();
    if (map && map->Instanceable())
        return false;

    LfgState state = sLFGMgr->GetState(bot->GetGUID());
    if (state != LFG_STATE_NONE)
        return false;

    return true;
}
