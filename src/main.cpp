#include <cmath>
#include <cstdlib>

#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/ShaderLayer.hpp>

// Animation state
static bool s_stopAnimations = true;
static bool s_disableShaders = false;
static bool s_renderEnabled = false;

// Player state
static float s_playerScale = 1.0f;
static int s_playerOpacity = 255;
static float s_playerRotation = 0.0f;

static bool s_scalePlayer = false;
static bool s_fadePlayer = false;
static bool s_rotatePlayer = false;

static bool s_reversePlayerScale = false;
static bool s_reversePlayerFade = false;

// Screen state
static int s_screenRotation = 28;
static float s_screenScale = 1.0f;
static float s_screenSkewX = 0.0f;
static float s_screenSkewY = 0.0f;
static float s_hueValue = 0.0f;

static bool s_spinScreen = false;
static bool s_scaleScreen = false;
static bool s_skewXScreen = false;
static bool s_skewYScreen = false;
static bool s_randomPause = false;
static bool s_hueShift = false;
static bool s_blackAndWhite = false;

static bool s_reverseScreenScale = false;
static bool s_reverseScreenSkewX = false;
static bool s_reverseScreenSkewY = false;

// Grayscale shader (matches cocos2d-x ccPositionTextureColor pattern)
// Note: compileShader prepends CC_MVPMatrix and other matrix uniforms,
// but NOT CC_Texture0 fragment shader must declare it explicitly
static const char *s_grayscaleVert =
	"attribute vec4 a_position;\n"
	"attribute vec2 a_texCoord;\n"
	"attribute vec4 a_color;\n"
	"#ifdef GL_ES\n"
	"varying lowp vec4 v_fragmentColor;\n"
	"varying mediump vec2 v_texCoord;\n"
	"#else\n"
	"varying vec4 v_fragmentColor;\n"
	"varying vec2 v_texCoord;\n"
	"#endif\n"
	"void main() {\n"
	"    gl_Position = CC_MVPMatrix * a_position;\n"
	"    v_fragmentColor = a_color;\n"
	"    v_texCoord = a_texCoord;\n"
	"}\n";

static const char *s_grayscaleFrag =
	"#ifdef GL_ES\n"
	"precision lowp float;\n"
	"#endif\n"
	"varying vec4 v_fragmentColor;\n"
	"varying vec2 v_texCoord;\n"
	"uniform sampler2D CC_Texture0;\n"
	"void main() {\n"
	"    vec4 c = v_fragmentColor * texture2D(CC_Texture0, v_texCoord);\n"
	"    float gray = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
	"    gl_FragColor = vec4(gray, gray, gray, c.a);\n"
	"}\n";

static ccColor3B hueToRGB(float hue) {
	float h = fmodf(hue, 360.0f);
	if (h < 0.0f) h += 360.0f;

	float x = 1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f);
	float r, g, b;

	if (h < 60.0f)        { r = 1; g = x; b = 0; }
	else if (h < 120.0f)  { r = x; g = 1; b = 0; }
	else if (h < 180.0f)  { r = 0; g = 1; b = x; }
	else if (h < 240.0f)  { r = 0; g = x; b = 1; }
	else if (h < 300.0f)  { r = x; g = 0; b = 1; }
	else                   { r = 1; g = 0; b = x; }

	return ccc3(
		static_cast<GLubyte>(r * 255),
		static_cast<GLubyte>(g * 255),
		static_cast<GLubyte>(b * 255)
	);
}

class $modify(PlayerObject) {
	bool init(int p0, int p1, GJBaseGameLayer *p2, cocos2d::CCLayer *p3,
			  bool p4) {
		s_scalePlayer = Mod::get()->getSettingValue<bool>("scalePlayer");
		s_fadePlayer = Mod::get()->getSettingValue<bool>("fadePlayer");
		s_rotatePlayer = Mod::get()->getSettingValue<bool>("spinPlayer");

		return PlayerObject::init(p0, p1, p2, p3, p4);
	}

	void update(float dt) {
		if (!s_stopAnimations) {
			if (s_scalePlayer) {
				if (s_playerScale >= 2.0f) s_reversePlayerScale = true;
				else if (s_playerScale <= 0.25f) s_reversePlayerScale = false;

				s_playerScale += s_reversePlayerScale ? -0.05f : 0.05f;
				PlayerObject::setScale(s_playerScale);
			}

			if (s_fadePlayer) {
				if (s_playerOpacity >= 255) s_reversePlayerFade = true;
				else if (s_playerOpacity <= 0) s_reversePlayerFade = false;

				s_playerOpacity += s_reversePlayerFade ? -3 : 3;
				s_playerOpacity = std::max(0, std::min(255, s_playerOpacity));
				PlayerObject::setOpacity(static_cast<unsigned char>(s_playerOpacity));
			}

			if (s_rotatePlayer) {
				s_playerRotation = fmodf(s_playerRotation + 1.0f, 360.0f);
				PlayerObject::setRRotation(s_playerRotation);
			}
		}

		PlayerObject::update(dt);
	}
};

class $modify(MyPlayLayer, PlayLayer) {
	struct Fields {
		CCNode *m_container = nullptr;
		CCNode *m_rotatedMenuContainer = nullptr;
		CCRenderTexture *m_renderTexture = nullptr;
		CCSprite *m_renderTo = nullptr;

		~Fields() {
			if (m_renderTexture) m_renderTexture->release();
		}
	};

	void onEnterTransitionDidFinish() {
		PlayLayer::onEnterTransitionDidFinish();

		s_spinScreen = Mod::get()->getSettingValue<bool>("spinScreen");
		s_scaleScreen = Mod::get()->getSettingValue<bool>("scaleScreen");
		s_skewXScreen = Mod::get()->getSettingValue<bool>("skewXScreen");
		s_skewYScreen = Mod::get()->getSettingValue<bool>("skewYScreen");
		s_randomPause = Mod::get()->getSettingValue<bool>("randomPause");
		s_hueShift = Mod::get()->getSettingValue<bool>("hueShift");
		s_blackAndWhite = Mod::get()->getSettingValue<bool>("blackAndWhite");
		s_disableShaders = Mod::get()->getSettingValue<bool>("disableShaders");

		s_renderEnabled =
			s_spinScreen || s_scaleScreen || s_skewXScreen || s_skewYScreen ||
			s_hueShift || s_blackAndWhite;

		if (s_renderEnabled) {
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

			m_fields->m_renderTexture = CCRenderTexture::create(
				static_cast<int>(winSize.width),
				static_cast<int>(winSize.height));
			m_fields->m_renderTexture->retain();

			m_fields->m_renderTo = CCSprite::createWithTexture(
				m_fields->m_renderTexture->getSprite()->getTexture());
			m_fields->m_renderTo->setFlipY(true);
			m_fields->m_renderTo->setZOrder(1);
			m_fields->m_renderTo->setID("hard-mode-render-node-id"_spr);
			m_fields->m_renderTo->setPosition(winSize / 2.f);

			if (s_spinScreen)
				m_fields->m_renderTo->setRotation(s_screenRotation);
			if (s_scaleScreen)
				m_fields->m_renderTo->setScale(s_screenScale);
			if (s_skewXScreen)
				m_fields->m_renderTo->setSkewX(s_screenSkewX);
			if (s_skewYScreen)
				m_fields->m_renderTo->setSkewY(s_screenSkewY);

			if (s_blackAndWhite) {
				auto *program = new CCGLProgram();
				program->initWithVertexShaderByteArray(
					s_grayscaleVert, s_grayscaleFrag);
				program->addAttribute(
					kCCAttributeNamePosition, kCCVertexAttrib_Position);
				program->addAttribute(
					kCCAttributeNameColor, kCCVertexAttrib_Color);
				program->addAttribute(
					kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
				program->link();
				program->updateUniforms();
				m_fields->m_renderTo->setShaderProgram(program);
				program->release();
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

		if (s_randomPause) {
			this->schedule(
				schedule_selector(MyPlayLayer::checkRandomPause), 1.0f);
		}

		s_stopAnimations = false;
	}

	void updateRender(float dt) {
		if (!s_renderEnabled) return;

		MyPlayLayer *mpl = static_cast<MyPlayLayer *>(PlayLayer::get());
		if (!mpl || !mpl->m_fields->m_renderTexture ||
			!mpl->m_fields->m_renderTo)
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

		if (s_stopAnimations) return;

		if (s_spinScreen) {
			s_screenRotation = (s_screenRotation + 1) % 360;
			mpl->m_fields->m_renderTo->setRotation(s_screenRotation);
		}

		if (s_scaleScreen) {
			if (s_screenScale >= 2.0f) s_reverseScreenScale = true;
			else if (s_screenScale <= 1.0f) s_reverseScreenScale = false;

			s_screenScale += s_reverseScreenScale ? -0.1f : 0.1f;
			mpl->m_fields->m_renderTo->setScale(s_screenScale);
		}

		if (s_skewXScreen) {
			if (s_screenSkewX >= 50.0f) s_reverseScreenSkewX = true;
			else if (s_screenSkewX <= -50.0f) s_reverseScreenSkewX = false;

			s_screenSkewX += s_reverseScreenSkewX ? -1.5f : 1.5f;
			mpl->m_fields->m_renderTo->setSkewX(s_screenSkewX);
		}

		if (s_skewYScreen) {
			if (s_screenSkewY >= 50.0f) s_reverseScreenSkewY = true;
			else if (s_screenSkewY <= -50.0f) s_reverseScreenSkewY = false;

			s_screenSkewY += s_reverseScreenSkewY ? -1.5f : 1.5f;
			mpl->m_fields->m_renderTo->setSkewY(s_screenSkewY);
		}

		if (s_hueShift) {
			s_hueValue = fmodf(s_hueValue + 2.0f, 360.0f);
			mpl->m_fields->m_renderTo->setColor(hueToRGB(s_hueValue));
		}
	}

	void checkRandomPause(float dt) {
		if (s_stopAnimations || !s_randomPause) return;

		MyPlayLayer *mpl = static_cast<MyPlayLayer *>(PlayLayer::get());
		if (!mpl) return;

		if (rand() % 3 == 0) {
			mpl->pauseGame(false);
		}
	}

	void resetRenderState() {
		s_stopAnimations = true;

		if (!m_fields->m_renderTo) return;

		m_fields->m_renderTo->setRotation(0);
		m_fields->m_renderTo->setScale(1.0f);
		m_fields->m_renderTo->setSkewX(0.0f);
		m_fields->m_renderTo->setSkewY(0.0f);
		m_fields->m_renderTo->setColor(ccc3(255, 255, 255));

		if (s_blackAndWhite) {
			m_fields->m_renderTo->setShaderProgram(
				CCShaderCache::sharedShaderCache()->programForKey(
					kCCShader_PositionTextureColor));
		}
	}

	void reapplyShader() {
		if (!s_blackAndWhite || !m_fields->m_renderTo) return;

		auto *program = new CCGLProgram();
		program->initWithVertexShaderByteArray(
			s_grayscaleVert, s_grayscaleFrag);
		program->addAttribute(
			kCCAttributeNamePosition, kCCVertexAttrib_Position);
		program->addAttribute(
			kCCAttributeNameColor, kCCVertexAttrib_Color);
		program->addAttribute(
			kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
		program->link();
		program->updateUniforms();
		m_fields->m_renderTo->setShaderProgram(program);
		program->release();
	}

	void resume() {
		s_stopAnimations = false;
		reapplyShader();
		PlayLayer::resume();
	}

	void fullReset() {
		s_stopAnimations = false;
		reapplyShader();
		PlayLayer::fullReset();
	}

	void resetLevel() {
		s_stopAnimations = false;
		reapplyShader();
		PlayLayer::resetLevel();
	}

	void pauseGame(bool p0) {
		resetRenderState();
		PlayLayer::pauseGame(p0);
	}

	void levelComplete() {
		resetRenderState();
		PlayLayer::levelComplete();
	}

	void onQuit() {
		resetRenderState();
		PlayLayer::onQuit();
	}
};

class $modify(ShaderLayer) {
	void performCalculations() {
		if (s_disableShaders && s_renderEnabled && m_state.m_usesShaders) {
			m_state.m_usesShaders = false;
		}

		if (!(s_disableShaders && s_renderEnabled)) {
			ShaderLayer::performCalculations();
		}
	}
};
