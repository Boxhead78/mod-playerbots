/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "RacialsStrategy.h"

#include "Playerbots.h"

class RacialsStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    RacialsStrategyActionNodeFactory() { creators["lifeblood"] = &lifeblood; }

private:
    static ActionNode* lifeblood(PlayerbotAI* botAI)
    {
        return new ActionNode("lifeblood",
                              /*P*/ {},
                              /*A*/ { NextAction("gift of the naaru") },
                              /*C*/ {});
    }
};

void RacialsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("low health", { NextAction("lifeblood", ACTION_NORMAL + 5) }));
    triggers.push_back(
        new TriggerNode("medium aoe", { NextAction("war stomp", ACTION_NORMAL + 5) }));
    triggers.push_back(new TriggerNode(
        "low mana", { NextAction("arcane torrent", ACTION_NORMAL + 5) }));

    triggers.push_back(new TriggerNode(
        "generic boost", { NextAction("blood fury", ACTION_NORMAL + 5),
        NextAction("berserking", ACTION_NORMAL + 5),
        NextAction("use trinket", ACTION_NORMAL + 4) }));

    triggers.push_back(
        new TriggerNode("medium health", NextAction("rocket jump", ACTION_NORMAL + 5), nullptr));
    triggers.push_back(
        new TriggerNode("random", NextAction("feral lunge", ACTION_NORMAL + 5), nullptr));
    triggers.push_back(
        new TriggerNode("often", NextAction("darkflight", ACTION_NORMAL + 5), nullptr));
    triggers.push_back(
        new TriggerNode("enemy out of melee", NextAction("rocket barrage", ACTION_NORMAL + 5), nullptr));

}

RacialsStrategy::RacialsStrategy(PlayerbotAI* botAI) : Strategy(botAI)
{
    actionNodeFactories.Add(new RacialsStrategyActionNodeFactory());
}
