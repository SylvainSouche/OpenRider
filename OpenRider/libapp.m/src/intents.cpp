#include "app/intents.h"

namespace app {

void Intents::Push(Intent intent) { items_.push_back(std::move(intent)); }

void Intents::Push(IntentKind kind, int i, int j, double x, double y)
{
    Intent intent{kind};
    intent.i = i;
    intent.j = j;
    intent.x = x;
    intent.y = y;
    items_.push_back(std::move(intent));
}

void Intents::PushText(IntentKind kind, std::string text)
{
    Intent intent{kind};
    intent.text = std::move(text);
    items_.push_back(std::move(intent));
}

std::vector<Intent> Intents::Take()
{
    std::vector<Intent> taken;
    taken.swap(items_);
    return taken;
}

} // namespace app
