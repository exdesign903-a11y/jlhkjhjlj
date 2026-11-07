#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <algorithm>
using namespace geode::prelude;

struct Action { int frame; int button; bool down; bool p1; };

namespace Bot {
    inline std::vector<Action> macro;
    inline size_t idx = 0;
    inline bool recording = false, replaying = false;
    inline bool injecting = false, waitingKey = false;
    inline int bindKey = KEY_R;

    inline int frame(GJBaseGameLayer* l) { return l->m_gameState.m_currentProgress; }

    inline void rewind(int f) {
        idx = std::lower_bound(macro.begin(), macro.end(), f,
            [](const Action& a, int fr) { return a.frame < fr; }) - macro.begin();
    }
}

class $modify(GJBaseGameLayer) {
    void handleButton(bool down, int button, bool p1) {
        if (!Bot::injecting) {
            if (Bot::replaying) return;
            auto pl = PlayLayer::get();
            if (Bot::recording && pl && pl->m_isPracticeMode)
                Bot::macro.push_back({ Bot::frame(this), button, down, p1 });
        }
        GJBaseGameLayer::handleButton(down, button, p1);
    }

    void processCommands(float dt) {
        if (Bot::replaying) {
            int f = Bot::frame(this);
            while (Bot::idx < Bot::macro.size() && Bot::macro[Bot::idx].frame <= f) {
                auto& a = Bot::macro[Bot::idx++];
                Bot::injecting = true;
                GJBaseGameLayer::handleButton(a.down, a.button, a.p1);
                Bot::injecting = false;
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

class $modify(PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        int f = Bot::frame(this);
        if (Bot::recording && m_isPracticeMode) {
            Bot::macro.erase(
                std::remove_if(Bot::macro.begin(), Bot::macro.end(),
                    [f](const Action& a) { return a.frame >= f; }),
                Bot::macro.end());
        }
        if (Bot::replaying) Bot::rewind(f);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        float s = (Bot::recording && m_isPracticeMode)
            ? (float)Mod::get()->getSettingValue<double>("speed") : 1.f;
        CCDirector::get()->getScheduler()->setTimeScale(s);
    }

    void onQuit() {
        Bot::replaying = false;
        CCDirector::get()->getScheduler()->setTimeScale(1.f);
        PlayLayer::onQuit();
    }
};

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double ts) {
        if (down && !repeat) {
            if (Bot::waitingKey) {
                Bot::waitingKey = false;
                Bot::bindKey = key;
                Notification::create(
                    fmt::format("Бинд: {}", CCKeyboardDispatcher::get()->keyToString(key)),
                    NotificationIcon::Success)->show();
                return true;
            }
            auto pl = PlayLayer::get();
            if (pl && key == Bot::bindKey) {
                if (Bot::replaying) {
                    Bot::replaying = false;
                    Bot::injecting = true;
                    pl->handleButton(false, 1, true);
                    pl->handleButton(false, 1, false);
                    Bot::injecting = false;
                    Notification::create("Реплей остановлен", NotificationIcon::Info)->show();
                } else if (!Bot::macro.empty()) {
                    Bot::recording = false;
                    Bot::replaying = true;
                    pl->resetLevel();
                    Notification::create("Реплей запущен", NotificationIcon::Success)->show();
                }
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, ts);
    }
};

class $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = CCMenu::create();
        menu->setPosition({ 70.f, CCDirector::get()->getWinSize().height / 2 });
        menu->setLayout(ColumnLayout::create()->setGap(8.f));

        auto rec = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Запись", "bigFont.fnt", "GJ_button_01.png", .6f),
            [](CCMenuItem*) {
                Bot::recording = !Bot::recording;
                if (Bot::recording) { Bot::replaying = false; Bot::macro.clear(); Bot::idx = 0; }
                Notification::create(Bot::recording ? "Запись ВКЛ (новый макрос)" : "Запись ВЫКЛ",
                    NotificationIcon::Info)->show();
            });
        auto bind = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Бинд", "bigFont.fnt", "GJ_button_02.png", .6f),
            [](CCMenuItem*) {
                Bot::waitingKey = true;
                Notification::create("Нажмите клавишу...", NotificationIcon::Info)->show();
            });
        menu->addChild(rec);
        menu->addChild(bind);
        menu->updateLayout();
        this->addChild(menu);
    }
};
