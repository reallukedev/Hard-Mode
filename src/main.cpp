// geode build -- -D CMAKE_EXPORT_COMPILE_COMMANDS=1

#include <cstdlib>
#include <ctime>

#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/ShaderLayer.hpp>

// Variables

bool stopAnimations = true;
bool disableShaders = false;

// PlayerObject

float playerScaleValue = 1.0f;
unsigned char playerOpacityValue = 255;
float playerRotationValue = 0.0f;

bool scalePlayer = false;
bool fadePlayer = false;
bool rotatePlayer = false;

bool reversePlayerScaling = false;
bool reversePlayerFading = false;

class $modify(PlayerObject) {
	bool init(int p0, int p1, GJBaseGameLayer *p2, cocos2d::CCLayer *p3,
			  bool p4) {
		scalePlayer = Mod::get()->getSettingValue<bool>("scalePlayer");
		fadePlayer = Mod::get()->getSettingValue<bool>("fadePlayer");
		rotatePlayer = Mod::get()->getSettingValue<bool>("spinPlayer");

		return PlayerObject::init(p0, p1, p2, p3, p4);
	}

	void update(float p0) {
		if (!stopAnimations) {
			if (scalePlayer) {
				if (playerScaleValue >= 2.0f) {
					reversePlayerScaling = true;
				} else if (playerScaleValue <= 0.25f) {
					reversePlayerScaling = false;
				}

				if (reversePlayerScaling) {
					playerScaleValue -= 0.05f;
				} else {
					playerScaleValue += 0.05f;
				}

				PlayerObject::setScale(playerScaleValue);
			}
			if (fadePlayer) {
				if (playerOpacityValue >= 255) {
					reversePlayerFading = true;
				} else if (playerOpacityValue <= 0) {
					reversePlayerFading = false;
				}

				if (reversePlayerFading) {
					playerOpacityValue -= 3;
				} else {
					playerOpacityValue += 3;
				}

				PlayerObject::setOpacity(playerOpacityValue);
			}
			if (rotatePlayer) {
				playerRotationValue += 1.0f;

				if (playerRotationValue >= 360.0f) {
					playerRotationValue = 0.0f;
				}

				PlayerObject::setRRotation(playerRotationValue);
			}
		}

		PlayerObject::update(p0);
	}
};

// PlayLayer

int screenRotation = 28;
float screenScaleValue = 1.0f;
float screenSkewXValue = 0.0f;
float screenSkewYValue = 0.0f;

bool screenRotating = false;
bool screenScale = false;
bool screenSkewX = false;
bool screenSkewY = false;
bool randomPause = false;

bool reverseScreenScaling = false;
bool reverseScreenSkewingX = false;
bool reverseScreenSkewingY = false;

bool playLayerModEnabled = true;

class $modify(MyPlayLayer, PlayLayer) {
	struct Fields {
		CCNode *m_rotatedMenuContainer;
		CCNode *m_container;
		CCRenderTexture *m_renderTexture;
		CCSprite *m_renderTo;
		CCSize m_newDesignResolution;
		CCSize m_newScreenScale;
		CCSize m_oldDesignResolution;
		CCSize m_originalScreenScale;
		~Fields() {
			if (m_renderTexture)
				m_renderTexture->release();
		}
	};

	void onEnterTransitionDidFinish() {
		PlayLayer::onEnterTransitionDidFinish();

		screenRotating = Mod::get()->getSettingValue<bool>("spinScreen");
		screenScale = Mod::get()->getSettingValue<bool>("scaleScreen");
		screenSkewX = Mod::get()->getSettingValue<bool>("skewXScreen");
		screenSkewY = Mod::get()->getSettingValue<bool>("skewYScreen");
		randomPause = Mod::get()->getSettingValue<bool>("randomPause");
		disableShaders = Mod::get()->getSettingValue<bool>("disableShaders");

		playLayerModEnabled =
			screenRotating || screenScale || screenSkewX || screenSkewY;

		if (playLayerModEnabled) {
			CCSize winSize = CCDirector::get()->getWinSize();
			m_fields->m_container = CCNode::create();
			m_fields->m_container->setZOrder(100000);
			m_fields->m_container->setAnchorPoint({0.5f, 0.5f});
			m_fields->m_container->setContentSize(winSize);
			m_fields->m_container->setPosition(winSize / 2.f);
			m_fields->m_container->setID("container"_spr);

			m_fields->m_rotatedMenuContainer = CCNode::create();
			m_fields->m_rotatedMenuContainer->setAnchorPoint({0.5f, 0.5f});
			m_fields->m_rotatedMenuContainer->setContentSize(
				{winSize.height, winSize.width});
			m_fields->m_rotatedMenuContainer->setPosition(winSize / 2.f);
			m_fields->m_rotatedMenuContainer->setID(
				"rotated-menu-container"_spr);

			m_fields->m_renderTexture =
				CCRenderTexture::create(static_cast<int>(winSize.width),
										static_cast<int>(winSize.height));
			m_fields->m_renderTexture->retain();
			m_fields->m_renderTo = CCSprite::createWithTexture(
				m_fields->m_renderTexture->getSprite()->getTexture());
			m_fields->m_renderTo->setFlipY(true);
			m_fields->m_renderTo->setZOrder(1);
			m_fields->m_renderTo->setID("hard-mode-render-node-id"_spr);

			m_fields->m_renderTo->setPosition(winSize / 2.f);

			if (screenRotating) {
				m_fields->m_renderTo->setRotation(screenRotation);
			}
			if (screenScale) {
				m_fields->m_renderTo->setScale(screenScaleValue);
			}
			if (screenSkewX) {
				m_fields->m_renderTo->setSkewX(screenSkewXValue);
			}
			if (screenSkewY) {
				m_fields->m_renderTo->setSkewY(screenSkewYValue);
			}

			m_fields->m_container->addChild(m_fields->m_renderTo);

			m_fields->m_container->addChild(m_fields->m_rotatedMenuContainer);

			CCScene *currentScene = CCDirector::get()->m_pNextScene;
			currentScene->addChild(m_fields->m_container);

			m_fields->m_renderTo->scheduleUpdate();
			m_fields->m_renderTo->schedule(
				schedule_selector(MyPlayLayer::updateRender));

			setVisible(false);
		}

		CCSize winSize = CCDirector::get()->getWinSize();

		m_fields->m_renderTexture = CCRenderTexture::create(
			static_cast<int>(winSize.width), static_cast<int>(winSize.height));
		m_fields->m_renderTo = CCSprite::createWithTexture(
			m_fields->m_renderTexture->getSprite()->getTexture());

		m_fields->m_renderTo->scheduleUpdate();
		m_fields->m_renderTo->schedule(
			schedule_selector(MyPlayLayer::updateRender));

		setVisible(false);

		stopAnimations = false;
	}

	void updateRender(float p0) {
		MyPlayLayer *mpl = static_cast<MyPlayLayer *>(PlayLayer::get());

		if (playLayerModEnabled) {
			if (!mpl->m_fields->m_renderTexture)
				return;
			if (!mpl->m_fields->m_renderTo)
				return;

			mpl->m_fields->m_renderTexture->beginWithClear(0.0f, 0.0f, 0.0f,
														   1.0f);

			mpl->m_fields->m_container->setVisible(false);

			CCScene *currentScene = CCDirector::get()->m_pRunningScene;
			mpl->setVisible(true);
			currentScene->visit();
			mpl->setVisible(false);

			mpl->m_fields->m_container->setVisible(true);
			mpl->m_fields->m_renderTexture->end();
			mpl->m_fields->m_renderTo->setTexture(
				mpl->m_fields->m_renderTexture->getSprite()->getTexture());
		} else {
			MyPlayLayer::updateRender(p0);
		}

		if (!stopAnimations) {
			if (screenRotating) {
				screenRotation += 1;

				if (screenRotation >= 360) {
					screenRotation = 0;
				}

				if (mpl->m_fields->m_renderTo) {
					mpl->m_fields->m_renderTo->setRotation(screenRotation);
				}
			}
			if (screenScale) {
				if (screenScaleValue >= 2.0f) {
					reverseScreenScaling = true;
				} else if (screenScaleValue <= 1.0f) {
					reverseScreenScaling = false;
				}

				if (reverseScreenScaling) {
					screenScaleValue -= 0.1f;
				} else {
					screenScaleValue += 0.1f;
				}

				if (mpl->m_fields->m_renderTo) {
					mpl->m_fields->m_renderTo->setScale(screenScaleValue);
				}
			}
			if (screenSkewX) {
				if (screenSkewXValue >= 50.0f) {
					reverseScreenSkewingX = true;
				} else if (screenSkewXValue <= -50.0f) {
					reverseScreenSkewingX = false;
				}

				if (reverseScreenSkewingX) {
					screenSkewXValue -= 1.5f;
				} else {
					screenSkewXValue += 1.5f;
				}

				if (mpl->m_fields->m_renderTo) {
					mpl->m_fields->m_renderTo->setSkewX(screenSkewXValue);
				}
			}
			if (screenSkewY) {
				if (screenSkewYValue >= 50.0f) {
					reverseScreenSkewingY = true;
				} else if (screenSkewYValue <= -50.0f) {
					reverseScreenSkewingY = false;
				}

				if (reverseScreenSkewingY) {
					screenSkewYValue -= 1.5f;
				} else {
					screenSkewYValue += 1.5f;
				}

				if (mpl->m_fields->m_renderTo) {
					mpl->m_fields->m_renderTo->setSkewY(screenSkewYValue);
				}
			}

			if (randomPause) {
				if (rand() % 100 == 0) {
					mpl->pauseGame(false);
				}
			}
		}
	}

	void resetRenderTexture() {
		MyPlayLayer *mpl = static_cast<MyPlayLayer *>(PlayLayer::get());

		stopAnimations = true;

		if (!mpl->m_fields->m_renderTexture)
			return;
		if (!mpl->m_fields->m_renderTo)
			return;

		mpl->m_fields->m_renderTo->setRotation(0);
		mpl->m_fields->m_renderTo->setScale(1.0f);
		mpl->m_fields->m_renderTo->setSkewX(0.0f);
		mpl->m_fields->m_renderTo->setSkewY(0.0f);
	}

	void resume() {
		stopAnimations = false;

		PlayLayer::resume();
	}

	void fullReset() {
		stopAnimations = false;

		PlayLayer::fullReset();
	}

	void resetLevel() {
		stopAnimations = false;

		PlayLayer::resetLevel();
	}

	void pauseGame(bool p0) {
		resetRenderTexture();

		PlayLayer::pauseGame(p0);
	}

	void levelComplete() {
		resetRenderTexture();

		PlayLayer::levelComplete();
	}

	void onQuit() {
		resetRenderTexture();

		PlayLayer::onQuit();
	}
};

// Shaders
class $modify(NoShaderSLHook, ShaderLayer) {
	void performCalculations() {
		if (m_state.m_usesShaders) {
			m_state.m_usesShaders = !(disableShaders && (playLayerModEnabled));
		}

		if (!(disableShaders && (playLayerModEnabled))) {
			ShaderLayer::performCalculations();
		}
	}
};
