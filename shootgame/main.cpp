#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <fstream>

const int WINDOW_W = 800;
const int WINDOW_H = 600;
const float PI = 3.14159265f;

enum GameState { INTRO, HOME, PLAYING, GAMEOVER };

// 分数记录
std::vector<int> loadScores() {
    std::vector<int> scores;
    std::ifstream f("scores.dat");
    int s;
    while (f >> s) scores.push_back(s);
    return scores;
}

void saveScore(int score) {
    auto scores = loadScores();
    scores.push_back(score);
    std::sort(scores.begin(), scores.end(), std::greater<int>());
    if (scores.size() > 10) scores.resize(10);
    std::ofstream f("scores.dat");
    for (int s : scores) f << s << "\n";
}

// 动画
struct Animation {
    std::vector<sf::Texture> frames;
    float frameDuration;
    int currentFrame = 0;
    float timer = 0.f;
    bool loop;

    Animation(float duration = 0.08f, bool loop = true) : frameDuration(duration), loop(loop) {}

    bool loadFromDir(const std::string& dir, int maxFrames = 99, int startFrame = 0) {
        for (int i = startFrame; i < startFrame + maxFrames; i++) {
            sf::Texture tex;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s/frame_%04d.png", dir.c_str(), i);
            if (!tex.loadFromFile(buf)) break;
            tex.setSmooth(true);
            frames.push_back(tex);
        }
        return !frames.empty();
    }

    void reset() { currentFrame = 0; timer = 0.f; }

    const sf::Texture& update(float dt) {
        timer += dt;
        if (timer >= frameDuration) {
            timer -= frameDuration;
            if (loop) {
                currentFrame = (currentFrame + 1) % frames.size();
            } else if (currentFrame < (int)frames.size() - 1) {
                currentFrame++;
            }
        }
        return frames[currentFrame];
    }

    bool isFinished() const {
        return !loop && currentFrame >= (int)frames.size() - 1;
    }
};

// 进化配置
struct Evolution {
    std::string name;
    std::string file;       // 静态立绘
    std::string animDir;    // 动画目录（空=用静态图）
    float scale;
    float speed;
    float shootCooldown;
    int bulletDamage;
    std::string effectDesc;
};

const Evolution EVOLUTIONS[] = {
    {"Exusiai",       "assets/exu1.png",  "frames/exu/idle",       0.48f, 3.0f, 0.15f, 20, ""},
    {"Exusiai Kai",   "assets/exu2.png",  "frames/exu/idle",       0.48f, 3.5f, 0.12f, 25, "20% AOE damage"},
    {"Covenant Exusiai", "assets/nexu1.png", "frames/flipped/idle", 0.5f, 4.0f, 0.10f, 30, "Triple shot every 5 attacks"},
    {"Covenant Exusiai Kai", "assets/nexu2.png", "frames/flipped/idle", 0.5f, 4.5f, 0.08f, 40, "Summon ally beacon every 20s"},
};
const int EVOLUTION_COUNT = 4;
const int EVOLUTION_SCORES[] = {0, 100, 300, 600};

// 玩家
struct Player {
    sf::Sprite sprite;
    float speed = 3.0f;
    int hp = 100;
    int maxHp = 100;
    float shootCooldown = 0.15f;
    float shootTimer = 0.f;
    int evolution = 0;
    int bulletDamage = 20;
    int attackCount = 0;
    float beaconCooldown = 0.f;
    bool useAnimation = false;

    // 能天使动画
    Animation exuIdleAnim;
    Animation exuAttackAnim;
    // 新约能天使动画
    Animation nexuIdleAnim;
    Animation nexuAttackAnim;
    Animation dieAnim;
    bool isAttacking = false;
    float attackAnimTimer = 0.f;

    Player() {
        sprite.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f);
    }
};

// 子弹
struct Bullet {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    float speed = 8.f;
    float lifetime = 2.f;
    bool isAOE = false;
    bool isAlly = false;

    Bullet(sf::Vector2f pos, sf::Vector2f dir, bool aoe = false, bool ally = false)
        : isAOE(aoe), isAlly(ally) {
        if (aoe) {
            shape.setRadius(6.f);
            shape.setFillColor(sf::Color(100, 50, 0));
            shape.setOrigin(6.f, 6.f);
        } else if (ally) {
            shape.setRadius(4.f);
            shape.setFillColor(sf::Color(0, 100, 180));
            shape.setOrigin(4.f, 4.f);
        } else {
            shape.setRadius(4.f);
            shape.setFillColor(sf::Color::Black);
            shape.setOrigin(4.f, 4.f);
        }
        shape.setPosition(pos);
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0) {
            velocity = sf::Vector2f(dir.x / len * speed, dir.y / len * speed);
        }
    }

    void update(float dt) {
        shape.move(velocity);
        lifetime -= dt;
    }
};

// 敌人
struct Enemy {
    sf::Sprite sprite;
    Animation anim;
    bool hasAnim = false;
    bool isRanged = false;
    bool isCharger = false;
    float speed = 1.2f;
    float baseSpeed = 1.2f;
    float maxSpeed = 4.0f;
    float acceleration = 0.5f;
    int hp = 30;
    int maxHp = 30;
    int damage = 10;
    float hitCooldown = 0.f;
    float shootTimer = 0.f;
    float shootCooldown = 2.0f;
    bool hasCharged = false; // For charger: whether first hit has been dealt

    Enemy(sf::Vector2f pos, Animation* animPtr = nullptr, int enemyType = 0) {
        isRanged = (enemyType == 1);
        isCharger = (enemyType == 2);

        if (animPtr && animPtr->frames.size() > 0) {
            hasAnim = true;
            anim = *animPtr;
            anim.loop = true;
            anim.reset();
            sprite.setTexture(anim.frames[0], true);
            float scale = isCharger ? 0.85f : 0.65f;
            sprite.setScale(scale, scale);
            sf::FloatRect bounds = sprite.getLocalBounds();
            sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
            sprite.setPosition(pos);
        } else {
            sprite.setPosition(pos);
        }
        if (isRanged) {
            shootTimer = shootCooldown;
        }
        if (isCharger) {
            speed = baseSpeed;
        }
    }

    bool update(sf::Vector2f targetPos, float dt) {
        if (hasAnim) {
            sprite.setTexture(anim.update(dt), true);
            sf::FloatRect bounds = sprite.getLocalBounds();
            sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        }
        sf::Vector2f dir = targetPos - sprite.getPosition();
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (isRanged) {
            // Ranged enemies stop at distance and shoot
            if (len > 150.f) {
                sprite.move(dir.x / len * speed, dir.y / len * speed);
            }
            shootTimer -= dt;
            if (shootTimer <= 0) {
                shootTimer = shootCooldown;
                return true; // Signal to shoot
            }
        } else if (isCharger) {
            // Charger: gradually accelerate
            if (len > 30.f) {
                speed = std::min(maxSpeed, speed + acceleration * dt);
                sprite.move(dir.x / len * speed, dir.y / len * speed);
            }
        } else {
            if (len > 0) {
                sprite.move(dir.x / len * speed, dir.y / len * speed);
            }
        }
        return false;
    }

    int getChargeDamage() const {
        if (isCharger && !hasCharged) {
            return static_cast<int>(speed * 5.f); // Extra damage based on speed
        }
        return 0;
    }

    sf::Vector2f getPosition() const {
        return sprite.getPosition();
    }
};

// Boss - 复仇者
struct Boss {
    sf::Sprite sprite;
    Animation idleAnim;
    Animation attackAnim;
    Animation reviveAnim;
    float speed = 1.5f;
    float baseSpeed = 1.5f;
    float chargeSpeed = 6.0f;
    int hp = 500;
    int maxHp = 500;
    int damage = 25;
    int enragedDamage = 50;
    float hitCooldown = 0.f;
    bool isEnraged = false;
    bool hasRevived = false;
    bool isCharging = false;
    float chargeTimer = 0.f;
    float chargeCooldown = 5.0f;
    float chargeDuration = 2.0f;
    sf::Vector2f chargeTarget;
    int animState = 0; // 0=idle, 1=attack, 2=revive

    Boss(sf::Vector2f pos, Animation* idle, Animation* attack, Animation* revive) {
        if (idle && idle->frames.size() > 0) {
            idleAnim = *idle;
            idleAnim.loop = true;
            idleAnim.reset();
            sprite.setTexture(idleAnim.frames[0], true);
        }
        if (attack && attack->frames.size() > 0) {
            attackAnim = *attack;
            attackAnim.loop = false;
        }
        if (revive && revive->frames.size() > 0) {
            reviveAnim = *revive;
            reviveAnim.loop = false;
        }
        sprite.setScale(0.8f, 0.8f);
        sf::FloatRect bounds = sprite.getLocalBounds();
        sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        sprite.setPosition(pos);
        chargeTimer = chargeCooldown;
    }

    bool update(sf::Vector2f targetPos, float dt) {
        // Update animation
        if (animState == 2) {
            sprite.setTexture(reviveAnim.update(dt), true);
            if (reviveAnim.isFinished()) {
                animState = 0;
                reviveAnim.reset();
            }
        } else if (animState == 1) {
            sprite.setTexture(attackAnim.update(dt), true);
            if (attackAnim.isFinished()) {
                animState = 0;
                attackAnim.reset();
            }
        } else {
            sprite.setTexture(idleAnim.update(dt), true);
        }
        sf::FloatRect bounds = sprite.getLocalBounds();
        sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);

        // Check enrage
        if (!isEnraged && hp < maxHp / 2) {
            isEnraged = true;
            damage = enragedDamage;
        }

        // Charge ability
        chargeTimer -= dt;
        if (isCharging) {
            chargeDuration -= dt;
            sf::Vector2f dir = chargeTarget - sprite.getPosition();
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 10.f && chargeDuration > 0) {
                sprite.move(dir.x / len * chargeSpeed, dir.y / len * chargeSpeed);
            } else {
                isCharging = false;
                chargeDuration = 2.0f;
                chargeTimer = chargeCooldown;
            }
        } else if (chargeTimer <= 0) {
            // Start charge
            isCharging = true;
            chargeTarget = targetPos;
            chargeDuration = 2.0f;
        } else {
            // Normal movement
            sf::Vector2f dir = targetPos - sprite.getPosition();
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0) {
                float currentSpeed = isEnraged ? speed * 1.3f : speed;
                sprite.move(dir.x / len * currentSpeed, dir.y / len * currentSpeed);
            }
        }

        return false;
    }

    bool takeDamage(int dmg) {
        hp -= dmg;
        if (hp <= 0 && !hasRevived) {
            // Revive
            hasRevived = true;
            hp = maxHp / 2;
            animState = 2;
            reviveAnim.reset();
            return false; // Not dead yet
        }
        return hp <= 0; // True if actually dead
    }

    sf::Vector2f getPosition() const {
        return sprite.getPosition();
    }

    int getCurrentDamage() const {
        return isEnraged ? enragedDamage : damage;
    }
};

// 盟军
struct Ally {
    sf::Sprite sprite;
    Animation anim;
    int hp = 100;
    int maxHp = 100;
    float shootTimer = 0.f;
    float shootCooldown = 2.65f; // Match animation duration
    bool hasAnim = false;

    Ally(sf::Vector2f pos, Animation* animPtr = nullptr) {
        if (animPtr && animPtr->frames.size() > 0) {
            hasAnim = true;
            anim = *animPtr;
            anim.loop = false;
            anim.reset();
            sprite.setTexture(anim.frames[0], true);
            sprite.setScale(0.4f, 0.4f);
            sf::FloatRect bounds = sprite.getLocalBounds();
            sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
            sprite.setPosition(pos);
        } else {
            sprite.setPosition(pos);
        }
    }

    bool update(float dt) {
        if (hasAnim) {
            sprite.setTexture(anim.update(dt), true);
            sf::FloatRect bounds = sprite.getLocalBounds();
            sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        }
        shootTimer -= dt;
        // Return true when animation finishes (time to shoot)
        if (hasAnim && anim.isFinished() && shootTimer <= 0) {
            anim.reset();
            shootTimer = shootCooldown;
            return true;
        }
        return false;
    }

    sf::Vector2f getPosition() const {
        return sprite.getPosition();
    }
};

// 信标
struct Beacon {
    sf::CircleShape shape;
    sf::CircleShape pulse;
    float lifetime = 3.f;
    float pulseRadius = 10.f;

    Beacon(sf::Vector2f pos) {
        shape.setRadius(8.f);
        shape.setFillColor(sf::Color(0, 255, 200));
        shape.setOrigin(8.f, 8.f);
        shape.setPosition(pos);

        pulse.setRadius(10.f);
        pulse.setFillColor(sf::Color::Transparent);
        pulse.setOutlineColor(sf::Color(0, 255, 200, 150));
        pulse.setOutlineThickness(2.f);
        pulse.setOrigin(10.f, 10.f);
        pulse.setPosition(pos);
    }

    bool update(float dt) {
        lifetime -= dt;
        pulseRadius += dt * 30.f;
        pulse.setRadius(pulseRadius);
        pulse.setOrigin(pulseRadius, pulseRadius);
        pulse.setPosition(shape.getPosition());
        pulse.setOutlineColor(sf::Color(0, 255, 200, (sf::Uint8)(150 * (lifetime / 3.f))));
        return lifetime > 0;
    }
};

// AOE爆炸
struct Explosion {
    sf::CircleShape shape;
    float lifetime = 0.3f;
    float radius;

    Explosion(sf::Vector2f pos, float r) : radius(r) {
        shape.setRadius(r);
        shape.setFillColor(sf::Color(255, 140, 0, 80));
        shape.setOutlineColor(sf::Color(255, 200, 0, 150));
        shape.setOutlineThickness(2.f);
        shape.setOrigin(r, r);
        shape.setPosition(pos);
    }

    bool update(float dt) {
        lifetime -= dt;
        float alpha = lifetime / 0.3f;
        shape.setFillColor(sf::Color(255, 140, 0, (sf::Uint8)(80 * alpha)));
        shape.setOutlineColor(sf::Color(255, 200, 0, (sf::Uint8)(150 * alpha)));
        return lifetime > 0;
    }
};

// 伤害数字
struct DamageText {
    sf::Text text;
    float lifetime = 0.6f;
    sf::Vector2f velocity;

    DamageText(sf::Vector2f pos, int dmg, sf::Font& font, sf::Color color = sf::Color::Yellow) {
        text.setFont(font);
        text.setString(std::to_string(dmg));
        text.setCharacterSize(16);
        text.setFillColor(color);
        text.setPosition(pos);
        velocity = sf::Vector2f((std::rand() % 60 - 30) / 10.f, -2.f);
    }

    void update(float dt) {
        text.move(velocity);
        velocity.y += 0.1f;
        lifetime -= dt;
    }
};

// 粒子
struct Particle {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    float lifetime;

    Particle(sf::Vector2f pos, sf::Color color) {
        shape.setRadius(2.f + std::rand() % 3);
        shape.setFillColor(color);
        shape.setPosition(pos);
        float angle = (std::rand() % 360) * PI / 180.f;
        float spd = 1.f + (std::rand() % 30) / 10.f;
        velocity = sf::Vector2f(std::cos(angle) * spd, std::sin(angle) * spd);
        lifetime = 0.3f + (std::rand() % 30) / 100.f;
    }

    void update(float dt) {
        shape.move(velocity);
        velocity *= 0.95f;
        lifetime -= dt;
    }
};

// 像素块爆炸效果
struct PixelBlock {
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    sf::Vector2f gravity;
    float rotation;
    float rotSpeed;
    float lifetime;
    float maxLifetime;

    PixelBlock(sf::Vector2f pos, sf::Color color) {
        float size = 3.f + std::rand() % 5;
        shape.setSize(sf::Vector2f(size, size));
        shape.setFillColor(color);
        shape.setOrigin(size / 2.f, size / 2.f);
        shape.setPosition(pos);

        float angle = (std::rand() % 360) * PI / 180.f;
        float spd = 2.f + (std::rand() % 50) / 10.f;
        velocity = sf::Vector2f(std::cos(angle) * spd, std::sin(angle) * spd);

        gravity = sf::Vector2f(0.f, 0.5f);
        rotation = std::rand() % 360;
        rotSpeed = (std::rand() % 20 - 10) * 5.f;
        maxLifetime = 0.5f + (std::rand() % 30) / 100.f;
        lifetime = maxLifetime;
    }

    void update(float dt) {
        velocity += gravity * dt * 60.f;
        shape.move(velocity);
        rotation += rotSpeed * dt;
        shape.setRotation(rotation);
        lifetime -= dt;

        float alpha = lifetime / maxLifetime;
        sf::Color c = shape.getFillColor();
        c.a = static_cast<sf::Uint8>(255 * alpha);
        shape.setFillColor(c);
    }
};

// 子弹弹道粒子
struct BulletTrail {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    float lifetime;
    float maxLifetime;

    BulletTrail(sf::Vector2f pos, sf::Color color, sf::Vector2f bulletVel) {
        float radius = 1.f + std::rand() % 3;
        shape.setRadius(radius);
        shape.setOrigin(radius, radius);
        shape.setPosition(pos);

        float spread = 0.3f;
        velocity = sf::Vector2f(
            bulletVel.x * (0.2f + (std::rand() % 50) / 100.f) + (std::rand() % 20 - 10) * spread,
            bulletVel.y * (0.2f + (std::rand() % 50) / 100.f) + (std::rand() % 20 - 10) * spread
        );

        sf::Uint8 r = std::min(255, color.r + std::rand() % 40);
        sf::Uint8 g = std::min(255, color.g + std::rand() % 40);
        sf::Uint8 b = std::min(255, color.b + std::rand() % 40);
        shape.setFillColor(sf::Color(r, g, b));

        maxLifetime = 0.15f + (std::rand() % 20) / 100.f;
        lifetime = maxLifetime;
    }

    void update(float dt) {
        shape.move(velocity);
        velocity *= 0.92f;
        lifetime -= dt;

        float alpha = lifetime / maxLifetime;
        sf::Color c = shape.getFillColor();
        c.a = static_cast<sf::Uint8>(255 * alpha);
        shape.setFillColor(c);
    }
};

// 波次
struct WaveConfig {
    int enemyCount;
    float spawnInterval;
    float enemySpeed;
    int enemyHp;
};

WaveConfig getWaveConfig(int wave) {
    return {
        5 + wave * 3,
        std::max(0.3f, 1.5f - wave * 0.1f),
        1.0f + wave * 0.15f,
        25 + wave * 10
    };
}

sf::Vector2f getSpawnPos() {
    int side = std::rand() % 4;
    float margin = 30.f;
    switch (side) {
        case 0: return sf::Vector2f(std::rand() % WINDOW_W, -margin);
        case 1: return sf::Vector2f(std::rand() % WINDOW_W, WINDOW_H + margin);
        case 2: return sf::Vector2f(-margin, std::rand() % WINDOW_H);
        case 3: return sf::Vector2f(WINDOW_W + margin, std::rand() % WINDOW_H);
    }
    return sf::Vector2f(0, 0);
}

float distance(sf::Vector2f a, sf::Vector2f b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

Enemy* findNearestEnemy(sf::Vector2f pos, std::vector<Enemy>& enemies) {
    Enemy* nearest = nullptr;
    float minDist = 999999.f;
    for (auto& e : enemies) {
        float d = distance(pos, e.getPosition());
        if (d < minDist) { minDist = d; nearest = &e; }
    }
    return nearest;
}

void setupPlayerSprite(Player& player, sf::Texture textures[], bool texturesLoaded[]) {
    int evo = player.evolution;
    if (player.useAnimation) {
        Animation& idleAnim = (evo >= 2) ? player.nexuIdleAnim : player.exuIdleAnim;
        if (idleAnim.frames.size() > 0) {
            player.sprite.setTexture(idleAnim.frames[0], true);
            player.sprite.setScale(EVOLUTIONS[evo].scale, EVOLUTIONS[evo].scale);
        }
    } else if (texturesLoaded[evo]) {
        player.sprite.setTexture(textures[evo], true);
        player.sprite.setScale(EVOLUTIONS[evo].scale, EVOLUTIONS[evo].scale);
    }
    sf::FloatRect bounds = player.sprite.getLocalBounds();
    player.sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "Shoot Game");
    window.setFramerateLimit(60);

    // 字体
    sf::Font font;
    bool hasFont = font.loadFromFile("C:/Windows/Fonts/msyhbd.ttc");
    if (!hasFont) hasFont = font.loadFromFile("C:/Windows/Fonts/msyh.ttc");
    if (!hasFont) hasFont = font.loadFromFile("C:/Windows/Fonts/simhei.ttf");
    if (!hasFont) hasFont = font.loadFromFile("C:/Windows/Fonts/simsun.ttc");
    if (!hasFont) hasFont = font.loadFromFile("C:/Windows/Fonts/arial.ttf");

    // 加载静态立绘
    sf::Texture textures[EVOLUTION_COUNT];
    bool texturesLoaded[EVOLUTION_COUNT] = {false};
    for (int i = 0; i < EVOLUTION_COUNT; i++) {
        texturesLoaded[i] = textures[i].loadFromFile(EVOLUTIONS[i].file);
        if (texturesLoaded[i]) textures[i].setSmooth(true);
    }

    // 加载进化音效
    sf::SoundBuffer evoBuffer1, evoBuffer2, evoBuffer3;
    sf::Sound evoSound;
    bool hasEvoSound1 = evoBuffer1.loadFromFile("audio/evo1.wav");
    bool hasEvoSound2 = evoBuffer2.loadFromFile("audio/evo2.wav");
    bool hasEvoSound3 = evoBuffer3.loadFromFile("audio/evo3.wav");

    // 加载战斗语音
    sf::SoundBuffer combatVoiceBuffers[4];
    sf::Sound combatVoiceSound;
    bool hasCombatVoice[4] = {false};
    hasCombatVoice[0] = combatVoiceBuffers[0].loadFromFile("audio/battle1.wav");
    hasCombatVoice[1] = combatVoiceBuffers[1].loadFromFile("audio/battle2.wav");
    hasCombatVoice[2] = combatVoiceBuffers[2].loadFromFile("audio/battle3.wav");
    hasCombatVoice[3] = combatVoiceBuffers[3].loadFromFile("audio/battle4.wav");

    // 盟军射击音效
    sf::SoundBuffer allyShootBuffer;
    sf::Sound allyShootSound;
    bool hasAllyShootSound = allyShootBuffer.loadFromFile("audio/ally_shoot.wav");

    // 加载动画（新约系列）
    // 能天使动画
    Animation exuIdleAnim(0.08f, true);
    Animation exuAttackAnim(0.06f, false);
    bool hasExuAnim = exuIdleAnim.loadFromDir("frames/exu/idle", 32, 1);
    exuAttackAnim.loadFromDir("frames/exu/attack", 10, 1);

    // 新约能天使动画
    Animation nexuIdleAnim(0.08f, true);
    Animation nexuAttackAnim(0.06f, false);
    Animation dieAnim(0.1f, false);
    bool hasNexuAnim = nexuIdleAnim.loadFromDir("frames/flipped/idle_new", 24, 1);
    nexuAttackAnim.loadFromDir("frames/flipped/attack_new", 9, 1);
    dieAnim.loadFromDir("frames/flipped/die", 10, 1);

    // 盟军动画
    Animation allyAnim(0.08f, false);
    bool hasAllyAnim = allyAnim.loadFromDir("frames/ally", 32, 1);

    // 敌人动画
    Animation enemyAnim(0.08f, true);
    bool hasEnemyAnim = enemyAnim.loadFromDir("frames/enemy", 24, 1);

    // 远程敌人动画
    Animation enemyRangedAnim(0.08f, true);
    bool hasEnemyRangedAnim = enemyRangedAnim.loadFromDir("frames/enemy_ranged", 15, 1);

    // 冲锋敌人动画
    Animation enemyChargerAnim(0.08f, true);
    bool hasEnemyChargerAnim = enemyChargerAnim.loadFromDir("frames/enemy_charger", 9, 1);

    // Boss动画
    Animation bossIdleAnim(0.1f, true);
    Animation bossAttackAnim(0.08f, false);
    Animation bossReviveAnim(0.1f, false);
    bool hasBossAnim = bossIdleAnim.loadFromDir("frames/boss/idle", 12, 1);
    bossAttackAnim.loadFromDir("frames/boss/attack", 21, 1);
    bossReviveAnim.loadFromDir("frames/boss/revive", 11, 1);

    // 死亡动画
    Animation deathAnim(0.12f, false);
    bool hasDeathAnim = deathAnim.loadFromDir("frames/death", 311, 1);
    sf::SoundBuffer deathAudioBuffer;
    sf::Sound deathAudioSound;
    bool hasDeathAudio = deathAudioBuffer.loadFromFile("audio/death_audio.wav");

    // 开场动画
    Animation introAnim(0.12f, false);
    bool hasIntroAnim = introAnim.loadFromDir("frames/intro", 51, 1);
    sf::SoundBuffer introAudioBuffer;
    sf::Sound introAudioSound;
    bool hasIntroAudio = introAudioBuffer.loadFromFile("audio/intro_audio.wav");
    bool introStarted = false;

    bool hasAnim = hasExuAnim || hasNexuAnim;

    // 游戏对象
    Player player;
    player.exuIdleAnim = exuIdleAnim;
    player.exuAttackAnim = exuAttackAnim;
    player.nexuIdleAnim = nexuIdleAnim;
    player.nexuAttackAnim = nexuAttackAnim;
    player.dieAnim = dieAnim;
    if (hasExuAnim) player.useAnimation = true;
    setupPlayerSprite(player, textures, texturesLoaded);

    std::vector<Bullet> bullets;
    std::vector<Bullet> enemyBullets;
    std::vector<Enemy> enemies;
    std::vector<Boss> bosses;
    std::vector<Ally> allies;
    std::vector<Beacon> beacons;
    std::vector<Explosion> explosions;
    std::vector<DamageText> damageTexts;
    std::vector<Particle> particles;
    std::vector<PixelBlock> pixelBlocks;
    std::vector<BulletTrail> bulletTrails;

    std::vector<int> scoreHistory = loadScores();
    int highestScore = scoreHistory.empty() ? 0 : scoreHistory[0];
    int score = 0;
    int wave = 1;
    int enemiesSpawned = 0;
    int enemiesKilled = 0;
    float spawnTimer = 0.f;
    GameState state = hasIntroAnim ? INTRO : HOME;
    bool gameOver = false;
    bool paused = false;
    float evoTextTimer = 0.f;
    float skillDescTimer = 0.f;
    bool bossSpawned = false;
    float combatVoiceTimer = 5.f + std::rand() % 6; // 5-10s initial delay
    bool deathAnimStarted = false;

    WaveConfig waveConfig = getWaveConfig(wave);

    // UI
    sf::Text hpText, scoreText, waveText, gameOverText, restartText, evoText, formText, skillText;
    if (hasFont) {
        hpText.setFont(font); hpText.setCharacterSize(18); hpText.setFillColor(sf::Color::Black);
        scoreText.setFont(font); scoreText.setCharacterSize(18); scoreText.setFillColor(sf::Color::Black);
        waveText.setFont(font); waveText.setCharacterSize(22); waveText.setFillColor(sf::Color(180, 120, 0));
        gameOverText.setFont(font); gameOverText.setCharacterSize(48); gameOverText.setFillColor(sf::Color::Red);
        gameOverText.setString("GAME OVER");
        restartText.setFont(font); restartText.setCharacterSize(20); restartText.setFillColor(sf::Color::Black);
        restartText.setString("Press R to restart");
        evoText.setFont(font); evoText.setCharacterSize(32); evoText.setFillColor(sf::Color(200, 150, 0));
        formText.setFont(font); formText.setCharacterSize(16); formText.setFillColor(sf::Color(80, 80, 150));
        skillText.setFont(font); skillText.setCharacterSize(15); skillText.setFillColor(sf::Color(180, 120, 50));
    }

    // 准心
    sf::CircleShape crosshair(8.f);
    crosshair.setFillColor(sf::Color::Transparent);
    crosshair.setOutlineColor(sf::Color(100, 100, 100, 150));
    crosshair.setOutlineThickness(1.5f);
    crosshair.setOrigin(8.f, 8.f);
    sf::Vertex crossLines[4];

    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        sf::Vector2f mousePos = window.mapPixelToCoords(mousePixel);
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (state == INTRO) {
                    state = HOME;
                    if (hasIntroAudio) introAudioSound.stop();
                    clock.restart();
                    continue;
                }
                if (state == HOME) {
                    float bx = WINDOW_W / 2.f - 75;
                    float by = WINDOW_H / 2.f + 20;
                    if (mousePos.x >= bx && mousePos.x <= bx + 150 && mousePos.y >= by && mousePos.y <= by + 45) {
                        player = Player();
                        player.exuIdleAnim = exuIdleAnim;
                        player.exuAttackAnim = exuAttackAnim;
                        player.nexuIdleAnim = nexuIdleAnim;
                        player.nexuAttackAnim = nexuAttackAnim;
                        player.dieAnim = dieAnim;
                        if (hasExuAnim) player.useAnimation = true;
                        setupPlayerSprite(player, textures, texturesLoaded);
                        bullets.clear(); enemyBullets.clear(); enemies.clear(); bosses.clear(); allies.clear();
                        beacons.clear(); explosions.clear();
                        damageTexts.clear(); particles.clear();
                        pixelBlocks.clear(); bulletTrails.clear();
                        score = 0; wave = 1; enemiesSpawned = 0; enemiesKilled = 0;
                        spawnTimer = 0; gameOver = false; evoTextTimer = 0; skillDescTimer = 0; bossSpawned = false;
                        combatVoiceTimer = 5.f + std::rand() % 6;
                        deathAnimStarted = false;
                        if (hasDeathAudio) deathAudioSound.stop();
                        waveConfig = getWaveConfig(wave);
                        state = PLAYING;
                        clock.restart();
                        continue;
                    }
                }
                if (state == GAMEOVER) {
                    saveScore(score);
                    scoreHistory = loadScores();
                    highestScore = scoreHistory.empty() ? 0 : scoreHistory[0];
                    state = HOME;
                    clock.restart();
                    continue;
                }
            }
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::P && state == PLAYING) paused = !paused;
                if (event.key.code == sf::Keyboard::Escape && state == HOME) window.close();

                // 快速切换进化等级 (1-4)
                if (state == PLAYING) {
                    int targetEvo = -1;
                    if (event.key.code == sf::Keyboard::Num1) targetEvo = 0;
                    if (event.key.code == sf::Keyboard::Num2) targetEvo = 1;
                    if (event.key.code == sf::Keyboard::Num3) targetEvo = 2;
                    if (event.key.code == sf::Keyboard::Num4) targetEvo = 3;
                    if (targetEvo >= 0 && targetEvo < EVOLUTION_COUNT) {
                        player.evolution = targetEvo;
                        player.speed = EVOLUTIONS[targetEvo].speed;
                        player.shootCooldown = EVOLUTIONS[targetEvo].shootCooldown;
                        player.bulletDamage = EVOLUTIONS[targetEvo].bulletDamage;
                        score = EVOLUTION_SCORES[targetEvo];
                        if (targetEvo <= 1 && hasExuAnim) {
                            player.useAnimation = true;
                        } else if (targetEvo >= 2 && hasNexuAnim) {
                            player.useAnimation = true;
                        }
                        setupPlayerSprite(player, textures, texturesLoaded);
                        evoTextTimer = 1.5f;
                        evoText.setString("Form: " + EVOLUTIONS[targetEvo].name);
                        // 播放进化音效
                        if (targetEvo == 1 && hasEvoSound1) {
                            evoSound.setBuffer(evoBuffer1);
                            evoSound.play();
                        } else if (targetEvo == 2 && hasEvoSound2) {
                            evoSound.setBuffer(evoBuffer2);
                            evoSound.play();
                        } else if (targetEvo == 3 && hasEvoSound3) {
                            evoSound.setBuffer(evoBuffer3);
                            evoSound.play();
                        }
                    }
                }
            }
        }

        // === INTRO 状态 ===
        if (state == INTRO) {
            window.clear(sf::Color::Black);
            if (!introStarted) {
                introAnim.reset();
                introStarted = true;
                if (hasIntroAudio) {
                    introAudioSound.setBuffer(introAudioBuffer);
                    introAudioSound.play();
                }
            }
            if (hasIntroAnim && !introAnim.isFinished()) {
                sf::Sprite introSprite;
                introSprite.setTexture(introAnim.update(dt), true);
                sf::FloatRect ib = introSprite.getLocalBounds();
                introSprite.setOrigin(ib.width / 2.f, ib.height / 2.f);
                introSprite.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f);
                window.draw(introSprite);
            } else {
                state = HOME;
                if (hasIntroAudio) introAudioSound.stop();
                clock.restart();
            }
            if (hasFont) {
                sf::Text skip("Click to skip", font, 14);
                skip.setFillColor(sf::Color(150, 150, 150));
                skip.setPosition(WINDOW_W / 2.f - 40, WINDOW_H - 40);
                window.draw(skip);
            }
            window.display();
            continue;
        }

        // === HOME 状态 ===
        if (state == HOME) {
            window.clear(sf::Color(20, 20, 40));
            if (hasFont) {
                // Title
                sf::Text title("COVENANT EXUSIAI", font, 36);
                title.setFillColor(sf::Color(255, 215, 0));
                title.setPosition(WINDOW_W / 2.f - 170, 80);
                window.draw(title);

                sf::Text subtitle("Shooting Game", font, 20);
                subtitle.setFillColor(sf::Color(200, 200, 200));
                subtitle.setPosition(WINDOW_W / 2.f - 65, 125);
                window.draw(subtitle);

                // Start button
                sf::RectangleShape btn(sf::Vector2f(150, 45));
                btn.setPosition(WINDOW_W / 2.f - 75, WINDOW_H / 2.f + 20);
                btn.setFillColor(sf::Color(50, 50, 100));
                btn.setOutlineColor(sf::Color(255, 215, 0));
                btn.setOutlineThickness(2.f);
                window.draw(btn);

                sf::Text btnText("START GAME", font, 18);
                btnText.setFillColor(sf::Color(255, 215, 0));
                btnText.setPosition(WINDOW_W / 2.f - 57, WINDOW_H / 2.f + 30);
                window.draw(btnText);

                // High score
                sf::Text highText("Highest Score: " + std::to_string(highestScore), font, 22);
                highText.setFillColor(sf::Color(255, 200, 100));
                highText.setPosition(WINDOW_W / 2.f - 95, WINDOW_H / 2.f - 80);
                window.draw(highText);

                // History
                sf::Text histTitle("Score History", font, 16);
                histTitle.setFillColor(sf::Color(150, 150, 180));
                histTitle.setPosition(WINDOW_W / 2.f - 50, WINDOW_H / 2.f + 90);
                window.draw(histTitle);

                int showCount = std::min(5, (int)scoreHistory.size());
                for (int i = 0; i < showCount; i++) {
                    sf::Text st(std::to_string(i + 1) + ". " + std::to_string(scoreHistory[i]), font, 14);
                    st.setFillColor(i == 0 ? sf::Color(255, 215, 0) : sf::Color(180, 180, 200));
                    st.setPosition(WINDOW_W / 2.f - 40, WINDOW_H / 2.f + 112 + i * 20);
                    window.draw(st);
                }

                sf::Text escText("ESC to quit", font, 12);
                escText.setFillColor(sf::Color(100, 100, 100));
                escText.setPosition(WINDOW_W / 2.f - 30, WINDOW_H - 30);
                window.draw(escText);
            }
            window.display();
            continue;
        }

        // 动画更新
        if (player.useAnimation && state == PLAYING) {
            bool isNexu = player.evolution >= 2;
            Animation& currentIdle = isNexu ? player.nexuIdleAnim : player.exuIdleAnim;
            Animation& currentAttack = isNexu ? player.nexuAttackAnim : player.exuAttackAnim;

            if (player.isAttacking) {
                player.attackAnimTimer -= dt;
                player.sprite.setTexture(currentAttack.update(dt), true);
                if (player.attackAnimTimer <= 0 || currentAttack.isFinished()) {
                    player.isAttacking = false;
                    currentAttack.reset();
                }
            } else {
                player.sprite.setTexture(currentIdle.update(dt), true);
            }
            sf::FloatRect b = player.sprite.getLocalBounds();
            player.sprite.setOrigin(b.width / 2.f, b.height / 2.f);
        }

        if (state == GAMEOVER && player.useAnimation) {
            player.sprite.setTexture(player.dieAnim.update(dt), true);
            sf::FloatRect b = player.sprite.getLocalBounds();
            player.sprite.setOrigin(b.width / 2.f, b.height / 2.f);
        }

        if (state == GAMEOVER || paused) {
            window.clear(sf::Color::White);
            for (auto& e : enemies) {
                if (e.hasAnim) window.draw(e.sprite);
            }
            for (auto& a : allies) {
                if (a.hasAnim) window.draw(a.sprite);
            }
            // Bosses
            for (auto& boss : bosses) window.draw(boss.sprite);
            window.draw(player.sprite);
            for (auto& b : bullets) window.draw(b.shape);
            for (auto& b : enemyBullets) window.draw(b.shape);

            // 死亡动画
            if (state == GAMEOVER && hasDeathAnim) {
                if (!deathAnimStarted) {
                    deathAnim.reset();
                    deathAnimStarted = true;
                    if (hasDeathAudio) {
                        deathAudioSound.setBuffer(deathAudioBuffer);
                        deathAudioSound.play();
                    }
                }
                if (!deathAnim.isFinished()) {
                    sf::Sprite deathSprite;
                    deathSprite.setTexture(deathAnim.update(dt), true);
                    sf::FloatRect db = deathSprite.getLocalBounds();
                    deathSprite.setOrigin(db.width / 2.f, db.height / 2.f);
                    deathSprite.setPosition(WINDOW_W / 2.f, WINDOW_H / 2.f);
                    window.draw(deathSprite);
                } else {
                    if (hasFont) {
                        gameOverText.setPosition(WINDOW_W / 2.f - 140, WINDOW_H / 2.f - 100);
                        window.draw(gameOverText);
                        scoreText.setString("Score: " + std::to_string(score));
                        scoreText.setPosition(WINDOW_W / 2.f - 50, WINDOW_H / 2.f - 50);
                        window.draw(scoreText);
                        formText.setString("Final Form: " + EVOLUTIONS[player.evolution].name);
                        formText.setPosition(WINDOW_W / 2.f - 80, WINDOW_H / 2.f - 30);
                        window.draw(formText);
                        sf::Text clickText("Click to return", font, 16);
                        clickText.setFillColor(sf::Color(100, 100, 100));
                        clickText.setPosition(WINDOW_W / 2.f - 55, WINDOW_H / 2.f + 10);
                        window.draw(clickText);
                    }
                }
            } else if (state == GAMEOVER && !hasDeathAnim) {
                if (hasFont) {
                    gameOverText.setPosition(WINDOW_W / 2.f - 140, WINDOW_H / 2.f - 100);
                    window.draw(gameOverText);
                    scoreText.setString("Score: " + std::to_string(score));
                    scoreText.setPosition(WINDOW_W / 2.f - 50, WINDOW_H / 2.f - 50);
                    window.draw(scoreText);
                    sf::Text clickText("Click to return", font, 16);
                    clickText.setFillColor(sf::Color(100, 100, 100));
                    clickText.setPosition(WINDOW_W / 2.f - 55, WINDOW_H / 2.f + 10);
                    window.draw(clickText);
                }
            } else {
                sf::Text t("PAUSED", font, 48);
                t.setFillColor(sf::Color::Black);
                t.setPosition(WINDOW_W / 2.f - 100, WINDOW_H / 2.f - 30);
                window.draw(t);
            }
            window.display();
            continue;
        }

        // === 更新 ===

        // 进化检查
        for (int i = EVOLUTION_COUNT - 1; i > 0; i--) {
            if (player.evolution < i && score >= EVOLUTION_SCORES[i]) {
                player.evolution = i;
                player.speed = EVOLUTIONS[i].speed;
                player.shootCooldown = EVOLUTIONS[i].shootCooldown;
                player.bulletDamage = EVOLUTIONS[i].bulletDamage;
                // 根据进化等级启用动画
                if (i <= 1 && hasExuAnim) {
                    player.useAnimation = true;
                } else if (i >= 2 && hasNexuAnim) {
                    player.useAnimation = true;
                }
                setupPlayerSprite(player, textures, texturesLoaded);
                player.hp = std::min(player.hp + 30, player.maxHp);
                evoTextTimer = 2.5f;
                skillDescTimer = 4.f;
                evoText.setString("EVOLUTION: " + EVOLUTIONS[i].name + "!");
                for (int p = 0; p < 20; p++)
                    particles.emplace_back(player.sprite.getPosition(), sf::Color(255, 215, 0));
                // 播放进化音效
                if (i == 1 && hasEvoSound1) {
                    evoSound.setBuffer(evoBuffer1);
                    evoSound.play();
                } else if (i == 2 && hasEvoSound2) {
                    evoSound.setBuffer(evoBuffer2);
                    evoSound.play();
                } else if (i == 3 && hasEvoSound3) {
                    evoSound.setBuffer(evoBuffer3);
                    evoSound.play();
                }
                break;
            }
        }
        evoTextTimer -= dt;
        skillDescTimer -= dt;

        // 战斗语音
        combatVoiceTimer -= dt;
        if (combatVoiceTimer <= 0) {
            int idx = std::rand() % 4;
            if (hasCombatVoice[idx]) {
                combatVoiceSound.setBuffer(combatVoiceBuffers[idx]);
                combatVoiceSound.play();
            }
            combatVoiceTimer = 5.f + std::rand() % 8; // 5-12s random interval
        }

        // 移动
        sf::Vector2f moveDir(0.f, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) moveDir.y -= 1;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) moveDir.y += 1;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) moveDir.x -= 1;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) moveDir.x += 1;
        float moveLen = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        if (moveLen > 0) player.sprite.move(moveDir.x / moveLen * player.speed, moveDir.y / moveLen * player.speed);
        sf::Vector2f pPos = player.sprite.getPosition();
        if (pPos.x < 30) player.sprite.setPosition(30, pPos.y);
        if (pPos.x > WINDOW_W - 30) player.sprite.setPosition(WINDOW_W - 30, pPos.y);
        if (pPos.y < 30) player.sprite.setPosition(pPos.x, 30);
        if (pPos.y > WINDOW_H - 30) player.sprite.setPosition(pPos.x, WINDOW_H - 30);

        // 射击
        player.shootTimer -= dt;
        if (state == PLAYING && sf::Mouse::isButtonPressed(sf::Mouse::Left) && player.shootTimer <= 0) {
            sf::Vector2f dir = mousePos - player.sprite.getPosition();
            sf::Vector2f pos = player.sprite.getPosition();

            player.attackCount++;
            // 触发攻击动画
            if (player.useAnimation) {
                player.isAttacking = true;
                player.attackAnimTimer = 0.3f;
                if (player.evolution >= 2) {
                    player.nexuAttackAnim.reset();
                } else {
                    player.exuAttackAnim.reset();
                }
            }

            if (player.evolution >= 2 && player.attackCount % 5 == 0) {
                float baseAngle = std::atan2(dir.y, dir.x);
                for (int i = -1; i <= 1; i++) {
                    float angle = baseAngle + i * 0.15f;
                    bullets.emplace_back(pos, sf::Vector2f(std::cos(angle), std::sin(angle)), false, false);
                }
            } else {
                bool isAOE = (player.evolution >= 1) && (std::rand() % 100 < 20);
                bullets.emplace_back(pos, dir, isAOE, false);
            }
            player.shootTimer = player.shootCooldown;
        }

        // 信标
        if (player.evolution >= 3) {
            player.beaconCooldown -= dt;
            if (player.beaconCooldown <= 0 && allies.empty()) {
                beacons.emplace_back(player.sprite.getPosition());
                player.beaconCooldown = 20.f;
            }
        }

        // 信标更新
        for (auto it = beacons.begin(); it != beacons.end(); ) {
            if (!it->update(dt)) {
                if (hasAllyAnim) {
                    allies.emplace_back(it->shape.getPosition(), &allyAnim);
                } else {
                    allies.emplace_back(it->shape.getPosition());
                }
                for (int p = 0; p < 10; p++)
                    particles.emplace_back(it->shape.getPosition(), sf::Color(0, 255, 200));
                it = beacons.erase(it);
            } else ++it;
        }

        // 盟军更新
        for (auto& ally : allies) {
            bool shouldShoot = ally.update(dt);
            Enemy* target = findNearestEnemy(ally.getPosition(), enemies);
            if (shouldShoot && target) {
                sf::Vector2f dir = target->getPosition() - ally.getPosition();
                bullets.emplace_back(ally.getPosition(), dir, false, true);
                if (hasAllyShootSound) {
                    allyShootSound.setBuffer(allyShootBuffer);
                    allyShootSound.play();
                }
            }
        }
        for (auto& e : enemies) {
            for (auto& ally : allies) {
                if (distance(e.getPosition(), ally.getPosition()) < 26.f && e.hitCooldown <= 0) {
                    ally.hp -= e.damage;
                    e.hitCooldown = 1.f;
                    for (int i = 0; i < 4; i++)
                        particles.emplace_back(ally.getPosition(), sf::Color(0, 180, 255));
                }
            }
        }
        allies.erase(std::remove_if(allies.begin(), allies.end(),
            [](const Ally& a) { return a.hp <= 0; }), allies.end());

        // 鼠标准心
        crosshair.setPosition(mousePos);
        float lineLen = 12.f;
        crossLines[0] = sf::Vertex(sf::Vector2f(mousePos.x - lineLen, mousePos.y), sf::Color(100,100,100,150));
        crossLines[1] = sf::Vertex(sf::Vector2f(mousePos.x + lineLen, mousePos.y), sf::Color(100,100,100,150));
        crossLines[2] = sf::Vertex(sf::Vector2f(mousePos.x, mousePos.y - lineLen), sf::Color(100,100,100,150));
        crossLines[3] = sf::Vertex(sf::Vector2f(mousePos.x, mousePos.y + lineLen), sf::Color(100,100,100,150));

        // 子弹更新 + 弹道特效
        for (auto& b : bullets) {
            sf::Vector2f oldPos = b.shape.getPosition();
            b.update(dt);
            // 每帧生成弹道粒子
            sf::Color trailColor = b.shape.getFillColor();
            bulletTrails.emplace_back(oldPos, trailColor, b.velocity);
            if (std::rand() % 3 == 0)
                bulletTrails.emplace_back(oldPos, trailColor, b.velocity);
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return b.lifetime <= 0 ||
                b.shape.getPosition().x < -20 || b.shape.getPosition().x > WINDOW_W + 20 ||
                b.shape.getPosition().y < -20 || b.shape.getPosition().y > WINDOW_H + 20; }),
            bullets.end());

        // 生成敌人
        if (enemiesSpawned < waveConfig.enemyCount) {
            spawnTimer -= dt;
            if (spawnTimer <= 0) {
                int enemyType = 0; // 0=normal, 1=ranged, 2=charger
                int roll = std::rand() % 100;
                if (roll < 20 && hasEnemyRangedAnim) {
                    enemyType = 1; // 20% chance for ranged
                } else if (roll < 35 && hasEnemyChargerAnim) {
                    enemyType = 2; // 15% chance for charger
                }
                Animation* animPtr = nullptr;
                if (enemyType == 1) {
                    animPtr = &enemyRangedAnim;
                } else if (enemyType == 2) {
                    animPtr = &enemyChargerAnim;
                } else if (hasEnemyAnim) {
                    animPtr = &enemyAnim;
                }
                Enemy e(getSpawnPos(), animPtr, enemyType);
                e.speed = waveConfig.enemySpeed;
                e.baseSpeed = waveConfig.enemySpeed;
                e.hp = waveConfig.enemyHp;
                e.maxHp = waveConfig.enemyHp;
                enemies.push_back(e);
                enemiesSpawned++;
                spawnTimer = waveConfig.spawnInterval;
            }
        }

        for (auto& e : enemies) {
            bool shouldShoot = e.update(player.sprite.getPosition(), dt);
            e.hitCooldown -= dt;
            if (shouldShoot) {
                sf::Vector2f dir = player.sprite.getPosition() - e.getPosition();
                Bullet b(e.getPosition(), dir, false, false);
                b.shape.setFillColor(sf::Color(0, 150, 0)); // Green for enemy bullets
                b.speed = 4.f;
                enemyBullets.push_back(b);
            }
        }

        // 子弹-敌人碰撞
        for (auto bi = bullets.begin(); bi != bullets.end(); ) {
            bool hit = false;
            for (auto ei = enemies.begin(); ei != enemies.end(); ) {
                if (distance(bi->shape.getPosition(), ei->getPosition()) < 18.f) {
                    int dmg = bi->isAlly ? 325 : player.bulletDamage;
                    if (bi->isAOE) {
                        float aoeRadius = 60.f;
                        explosions.emplace_back(bi->shape.getPosition(), aoeRadius);
                        for (auto& e2 : enemies) {
                            if (distance(bi->shape.getPosition(), e2.getPosition()) < aoeRadius) {
                                e2.hp -= dmg;
                                if (hasFont) damageTexts.emplace_back(e2.getPosition(), dmg, font, sf::Color(255, 140, 0));
                                for (int i = 0; i < 3; i++) particles.emplace_back(e2.getPosition(), sf::Color(255, 140, 0));
                            }
                        }
                        for (auto it = enemies.begin(); it != enemies.end(); ) {
                            if (it->hp <= 0) {
                                for (int i = 0; i < 15; i++) pixelBlocks.emplace_back(it->getPosition(), sf::Color(255, std::rand() % 100, 0));
                                for (int i = 0; i < 6; i++) particles.emplace_back(it->getPosition(), sf::Color::Yellow);
                                score += 10; enemiesKilled++;
                                it = enemies.erase(it);
                            } else ++it;
                        }
                        // AOE damage to bosses
                        for (auto& boss : bosses) {
                            if (distance(bi->shape.getPosition(), boss.getPosition()) < aoeRadius) {
                                bool dead = boss.takeDamage(dmg);
                                if (hasFont) damageTexts.emplace_back(boss.getPosition(), dmg, font, sf::Color(255, 140, 0));
                                for (int i = 0; i < 3; i++) particles.emplace_back(boss.getPosition(), sf::Color(255, 140, 0));
                                if (dead) {
                                    for (int i = 0; i < 25; i++) pixelBlocks.emplace_back(boss.getPosition(), sf::Color(200, 0, 0));
                                    score += 50;
                                }
                            }
                        }
                        bosses.erase(std::remove_if(bosses.begin(), bosses.end(),
                            [](const Boss& b) { return b.hp <= 0; }), bosses.end());
                    } else {
                        ei->hp -= dmg;
                        if (hasFont) damageTexts.emplace_back(ei->getPosition(), dmg, font, bi->isAlly ? sf::Color(0, 200, 255) : sf::Color::Yellow);
                        for (int i = 0; i < 5; i++) particles.emplace_back(ei->getPosition(), bi->isAlly ? sf::Color(0, 200, 255) : sf::Color::Yellow);
                        if (ei->hp <= 0) {
                            for (int i = 0; i < 15; i++) pixelBlocks.emplace_back(ei->getPosition(), sf::Color(255, std::rand() % 80, 0));
                            for (int i = 0; i < 6; i++) particles.emplace_back(ei->getPosition(), sf::Color::Yellow);
                            ei = enemies.erase(ei); score += 10; enemiesKilled++;
                        }
                    }
                    hit = true; break;
                } else ++ei;
            }
            if (hit) bi = bullets.erase(bi); else ++bi;
        }

        // 敌人-玩家碰撞
        for (auto& e : enemies) {
            if (distance(e.getPosition(), player.sprite.getPosition()) < 30.f && e.hitCooldown <= 0) {
                int totalDamage = e.damage;
                int chargeDmg = e.getChargeDamage();
                if (chargeDmg > 0) {
                    totalDamage += chargeDmg;
                    e.hasCharged = true;
                    // Extra particles for charge hit
                    for (int i = 0; i < 10; i++) particles.emplace_back(player.sprite.getPosition(), sf::Color::Red);
                    if (hasFont) damageTexts.emplace_back(player.sprite.getPosition(), chargeDmg, font, sf::Color::Red);
                }
                player.hp -= totalDamage;
                e.hitCooldown = 1.f;
                for (int i = 0; i < 6; i++) particles.emplace_back(player.sprite.getPosition(), sf::Color::Cyan);
                if (player.hp <= 0) { player.hp = 0; state = GAMEOVER; deathAnimStarted = false; }
            }
        }

        // Boss更新
        for (auto& boss : bosses) {
            boss.update(player.sprite.getPosition(), dt);
            boss.hitCooldown -= dt;
            // Boss charge attack animation trigger
            if (boss.isCharging && boss.animState != 1) {
                boss.animState = 1;
                boss.attackAnim.reset();
            }
            // Boss碰撞玩家
            if (distance(boss.getPosition(), player.sprite.getPosition()) < 35.f && boss.hitCooldown <= 0) {
                int dmg = boss.getCurrentDamage();
                player.hp -= dmg;
                boss.hitCooldown = 1.f;
                for (int i = 0; i < 8; i++) particles.emplace_back(player.sprite.getPosition(), sf::Color::Red);
                if (hasFont) damageTexts.emplace_back(player.sprite.getPosition(), dmg, font, sf::Color::Red);
                if (player.hp <= 0) { player.hp = 0; state = GAMEOVER; deathAnimStarted = false; }
            }
            // Boss碰撞盟军
            for (auto& ally : allies) {
                if (distance(boss.getPosition(), ally.getPosition()) < 30.f && boss.hitCooldown <= 0) {
                    ally.hp -= boss.getCurrentDamage();
                    boss.hitCooldown = 1.f;
                    for (int i = 0; i < 6; i++) particles.emplace_back(ally.getPosition(), sf::Color(0, 180, 255));
                }
            }
        }

        // 子弹-Boss碰撞
        for (auto bi = bullets.begin(); bi != bullets.end(); ) {
            bool hit = false;
            for (auto bsi = bosses.begin(); bsi != bosses.end(); ) {
                if (distance(bi->shape.getPosition(), bsi->getPosition()) < 30.f) {
                    int dmg = bi->isAlly ? 325 : player.bulletDamage;
                    if (bi->isAOE) {
                        float aoeRadius = 60.f;
                        explosions.emplace_back(bi->shape.getPosition(), aoeRadius);
                        for (auto& b2 : bosses) {
                            if (distance(bi->shape.getPosition(), b2.getPosition()) < aoeRadius) {
                                b2.takeDamage(dmg);
                                if (hasFont) damageTexts.emplace_back(b2.getPosition(), dmg, font, sf::Color(255, 140, 0));
                                for (int i = 0; i < 3; i++) particles.emplace_back(b2.getPosition(), sf::Color(255, 140, 0));
                            }
                        }
                        for (auto& e2 : enemies) {
                            if (distance(bi->shape.getPosition(), e2.getPosition()) < aoeRadius) {
                                e2.hp -= dmg;
                                if (hasFont) damageTexts.emplace_back(e2.getPosition(), dmg, font, sf::Color(255, 140, 0));
                                for (int i = 0; i < 3; i++) particles.emplace_back(e2.getPosition(), sf::Color(255, 140, 0));
                            }
                        }
                        bosses.erase(std::remove_if(bosses.begin(), bosses.end(),
                            [](const Boss& b) { return b.hp <= 0; }), bosses.end());
                        for (auto it = enemies.begin(); it != enemies.end(); ) {
                            if (it->hp <= 0) {
                                for (int i = 0; i < 15; i++) pixelBlocks.emplace_back(it->getPosition(), sf::Color(255, std::rand() % 100, 0));
                                score += 10; enemiesKilled++;
                                it = enemies.erase(it);
                            } else ++it;
                        }
                    } else {
                        bool dead = bsi->takeDamage(dmg);
                        if (hasFont) damageTexts.emplace_back(bsi->getPosition(), dmg, font, sf::Color(255, 100, 0));
                        for (int i = 0; i < 5; i++) particles.emplace_back(bsi->getPosition(), sf::Color(255, 100, 0));
                        if (dead) {
                            for (int i = 0; i < 25; i++) pixelBlocks.emplace_back(bsi->getPosition(), sf::Color(200, 0, 0));
                            for (int i = 0; i < 10; i++) particles.emplace_back(bsi->getPosition(), sf::Color::Yellow);
                            score += 50;
                            bsi = bosses.erase(bsi);
                        }
                    }
                    hit = true; break;
                } else ++bsi;
            }
            if (hit) bi = bullets.erase(bi); else ++bi;
        }

        // 敌人子弹更新
        for (auto& b : enemyBullets) b.update(dt);
        enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(),
            [](const Bullet& b) { return b.lifetime <= 0 ||
                b.shape.getPosition().x < -20 || b.shape.getPosition().x > WINDOW_W + 20 ||
                b.shape.getPosition().y < -20 || b.shape.getPosition().y > WINDOW_H + 20; }),
            enemyBullets.end());

        // 敌人子弹-玩家碰撞
        for (auto bi = enemyBullets.begin(); bi != enemyBullets.end(); ) {
            if (distance(bi->shape.getPosition(), player.sprite.getPosition()) < 20.f) {
                player.hp -= 15;
                for (int i = 0; i < 4; i++) particles.emplace_back(player.sprite.getPosition(), sf::Color::Red);
                bi = enemyBullets.erase(bi);
                if (player.hp <= 0) { player.hp = 0; state = GAMEOVER; deathAnimStarted = false; }
            } else ++bi;
        }

        // 波次
        if (enemiesKilled >= waveConfig.enemyCount && enemies.empty() && bosses.empty()) {
            wave++; enemiesSpawned = 0; enemiesKilled = 0;
            waveConfig = getWaveConfig(wave);
            player.hp = std::min(player.hp + 20, player.maxHp);
            bossSpawned = false;
        }

        // Boss生成 (每5波)
        if (wave % 5 == 0 && !bossSpawned && enemies.empty() && hasBossAnim) {
            Boss boss(getSpawnPos(), &bossIdleAnim, &bossAttackAnim, &bossReviveAnim);
            boss.hp = 500 + wave * 50;
            boss.maxHp = boss.hp;
            boss.speed = 1.5f + wave * 0.1f;
            bosses.push_back(boss);
            bossSpawned = true;
        }

        // 特效更新
        for (auto& p : particles) p.update(dt);
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.lifetime <= 0; }), particles.end());
        for (auto& d : damageTexts) d.update(dt);
        damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& d) { return d.lifetime <= 0; }), damageTexts.end());
        for (auto& ex : explosions) ex.update(dt);
        explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& e) { return e.lifetime <= 0; }), explosions.end());
        for (auto& pb : pixelBlocks) pb.update(dt);
        pixelBlocks.erase(std::remove_if(pixelBlocks.begin(), pixelBlocks.end(), [](const PixelBlock& pb) { return pb.lifetime <= 0; }), pixelBlocks.end());
        for (auto& bt : bulletTrails) bt.update(dt);
        bulletTrails.erase(std::remove_if(bulletTrails.begin(), bulletTrails.end(), [](const BulletTrail& bt) { return bt.lifetime <= 0; }), bulletTrails.end());

        // === 渲染 ===
        window.clear(sf::Color::White);

        // 网格
        for (int x = 0; x < WINDOW_W; x += 40) {
            sf::Vertex l[] = { sf::Vertex(sf::Vector2f(x, 0), sf::Color(230, 230, 230)), sf::Vertex(sf::Vector2f(x, WINDOW_H), sf::Color(230, 230, 230)) };
            window.draw(l, 2, sf::Lines);
        }
        for (int y = 0; y < WINDOW_H; y += 40) {
            sf::Vertex l[] = { sf::Vertex(sf::Vector2f(0, y), sf::Color(230, 230, 230)), sf::Vertex(sf::Vector2f(WINDOW_W, y), sf::Color(230, 230, 230)) };
            window.draw(l, 2, sf::Lines);
        }

        for (auto& ex : explosions) window.draw(ex.shape);
        for (auto& b : beacons) { window.draw(b.pulse); window.draw(b.shape); }
        for (auto& bt : bulletTrails) window.draw(bt.shape);
        for (auto& p : particles) window.draw(p.shape);
        for (auto& pb : pixelBlocks) window.draw(pb.shape);

        // 敌人
        for (auto& e : enemies) {
            if (e.hasAnim) {
                window.draw(e.sprite);
            } else {
                sf::CircleShape shape(14.f);
                shape.setFillColor(sf::Color::Red);
                shape.setOrigin(14.f, 14.f);
                shape.setPosition(e.getPosition());
                window.draw(shape);
            }
            float r = (float)e.hp / e.maxHp;
            sf::RectangleShape bg(sf::Vector2f(28, 3)); bg.setPosition(e.getPosition().x - 14, e.getPosition().y - 22); bg.setFillColor(sf::Color(60, 60, 60)); window.draw(bg);
            sf::RectangleShape bar(sf::Vector2f(28 * r, 3)); bar.setPosition(e.getPosition().x - 14, e.getPosition().y - 22); bar.setFillColor(r > 0.5f ? sf::Color::Green : sf::Color::Red); window.draw(bar);
        }

        // Boss
        for (auto& boss : bosses) {
            window.draw(boss.sprite);
            float r = (float)boss.hp / boss.maxHp;
            sf::Vector2f bp = boss.getPosition();
            sf::RectangleShape bg(sf::Vector2f(40, 5)); bg.setPosition(bp.x - 20, bp.y - 30); bg.setFillColor(sf::Color(60, 60, 60)); window.draw(bg);
            sf::RectangleShape bar(sf::Vector2f(40 * r, 5)); bar.setPosition(bp.x - 20, bp.y - 30);
            bar.setFillColor(boss.isEnraged ? sf::Color(255, 50, 50) : sf::Color(255, 180, 0)); window.draw(bar);
            if (boss.isCharging) {
                sf::CircleShape indicator(6.f);
                indicator.setFillColor(sf::Color(255, 0, 0, 150));
                indicator.setOrigin(6.f, 6.f);
                indicator.setPosition(bp.x, bp.y - 38);
                window.draw(indicator);
            }
        }

        // 盟军
        for (auto& a : allies) {
            if (a.hasAnim) {
                window.draw(a.sprite);
            } else {
                sf::CircleShape shape(12.f);
                shape.setFillColor(sf::Color(0, 180, 255));
                shape.setOrigin(12.f, 12.f);
                shape.setPosition(a.getPosition());
                window.draw(shape);
            }
            float r = (float)a.hp / a.maxHp;
            sf::Vector2f pos = a.getPosition();
            sf::RectangleShape bg(sf::Vector2f(24, 3)); bg.setPosition(pos.x - 12, pos.y - 20); bg.setFillColor(sf::Color(60, 60, 60)); window.draw(bg);
            sf::RectangleShape bar(sf::Vector2f(24 * r, 3)); bar.setPosition(pos.x - 12, pos.y - 20); bar.setFillColor(sf::Color(0, 200, 255)); window.draw(bar);
        }

        for (auto& b : bullets) window.draw(b.shape);
        for (auto& b : enemyBullets) window.draw(b.shape);
        window.draw(player.sprite);
        window.draw(crosshair);
        window.draw(crossLines, 4, sf::Lines);
        for (auto& d : damageTexts) window.draw(d.text);

        // 进化提示
        if (hasFont && evoTextTimer > 0) {
            evoText.setPosition(WINDOW_W / 2.f - 200, WINDOW_H / 2.f - 100);
            evoText.setFillColor(sf::Color(255, 215, 0, (sf::Uint8)(255 * std::min(1.f, evoTextTimer))));
            window.draw(evoText);
        }
        if (hasFont && skillDescTimer > 0 && player.evolution > 0) {
            sf::Text dt2("New Skill: " + EVOLUTIONS[player.evolution].effectDesc, font, 20);
            dt2.setFillColor(sf::Color(255, 200, 100, (sf::Uint8)(255 * std::min(1.f, skillDescTimer))));
            dt2.setPosition(WINDOW_W / 2.f - 180, WINDOW_H / 2.f - 60);
            window.draw(dt2);
        }

        // UI
        if (hasFont) {
            hpText.setString("HP: " + std::to_string(player.hp) + "/" + std::to_string(player.maxHp));
            hpText.setPosition(10, 10); window.draw(hpText);
            float r = (float)player.hp / player.maxHp;
            sf::RectangleShape bg(sf::Vector2f(200, 12)); bg.setPosition(10, 35); bg.setFillColor(sf::Color(60, 60, 60)); window.draw(bg);
            sf::RectangleShape bar(sf::Vector2f(200 * r, 12)); bar.setPosition(10, 35); bar.setFillColor(r > 0.5f ? sf::Color::Green : (r > 0.25f ? sf::Color::Yellow : sf::Color::Red)); window.draw(bar);

            scoreText.setString("Score: " + std::to_string(score));
            scoreText.setPosition(WINDOW_W - 150, 10); window.draw(scoreText);
            waveText.setString("Wave " + std::to_string(wave));
            waveText.setPosition(WINDOW_W / 2.f - 40, 10); window.draw(waveText);

            formText.setString(EVOLUTIONS[player.evolution].name);
            formText.setPosition(10, 55); window.draw(formText);

            for (int i = 1; i <= player.evolution; i++) {
                skillText.setString("[Lv" + std::to_string(i + 1) + "] " + EVOLUTIONS[i].effectDesc);
                skillText.setPosition(10, 75 + (i - 1) * 18);
                window.draw(skillText);
            }

            if (player.evolution < EVOLUTION_COUNT - 1) {
                sf::Text nt; nt.setFont(font); nt.setCharacterSize(14); nt.setFillColor(sf::Color(120, 120, 120));
                nt.setString("Next: " + std::to_string(EVOLUTION_SCORES[player.evolution + 1]) + " pts");
                nt.setPosition(10, 75 + player.evolution * 18); window.draw(nt);
            } else {
                sf::Text mt; mt.setFont(font); mt.setCharacterSize(14); mt.setFillColor(sf::Color(200, 150, 0));
                mt.setString("MAX EVOLUTION"); mt.setPosition(10, 75 + (EVOLUTION_COUNT - 1) * 18); window.draw(mt);
            }

            if (player.evolution >= 3) {
                sf::Text bt; bt.setFont(font); bt.setCharacterSize(14);
                float cd = std::max(0.f, player.beaconCooldown);
                if (!allies.empty()) {
                    bt.setString("Ally HP: " + std::to_string(allies[0].hp));
                    bt.setFillColor(sf::Color(0, 100, 180));
                } else {
                    bt.setString(cd > 0 ? "Beacon: " + std::to_string((int)cd) + "s" : "Beacon: Ready");
                    bt.setFillColor(sf::Color(0, 150, 120));
                }
                bt.setPosition(10, WINDOW_H - 30); window.draw(bt);
            }
        }

        window.display();
    }

    return 0;
}
