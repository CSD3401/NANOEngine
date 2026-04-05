#pragma once
#include "EngineAPI.hpp"
#include <algorithm>
#include <ScriptSDK/UI.h>

#define GLFW_MOUSE_BUTTON_LEFT 0

/**
 * Cutscene_Controller
 *
 * Displays a series of cutscene "pages" (UI image GameObjects) one at a time.
 * Each page has a matching caption, an optional audio clip, and an optional
 * custom display duration. A BGM track plays for the full duration of the
 * cutscene. A left mouse click advances to the next page early.
 *
 * ── Inspector Fields ─────────────────────────────────────────────────────────
 *
 *  eventName        (String) Event that triggers this cutscene.
 *                            e.g. "StartCutscene_Intro"
 *
 *  bgmAudioName     (String) FMOD event path for background music.
 *                            Plays when cutscene starts, stops when it ends.
 *                            e.g. "event:/BGM_CutsceneIntro"
 *                            Leave empty for no BGM.
 *
 *  autoAdvance      (Bool)   Enable timed page flipping.
 *
 *  autoAdvanceDelay (Float)  Default seconds per page used when a page has no
 *                            entry in pageDelays, or pageDelays is empty.
 *                            Default: 2.0s.
 *
 *  pageDelays       (Float Vector) Per-page display duration in seconds.
 *                            Index matches pageImages order.
 *                            Any missing entries fall back to autoAdvanceDelay.
 *                            e.g. [2.0, 5.0, 3.0]
 *
 *  pageAudioNames   (String Vector) FMOD event path to play on each page.
 *                            Index matches pageImages order.
 *                            Leave an entry empty for a silent page.
 *                            Stopped automatically when the page changes.
 *                            e.g. ["event:/VO_Intro1", "event:/VO_Intro2"]
 *
 *  pageCaptions     (String Vector) Caption text per page.
 *                            Leave an entry empty for no caption on that page.
 *
 *  captionTextRef   (GameObject) Shared UIText entity for captions.
 *                            Set INACTIVE in editor.
 *
 *  pageImages       (GameObject Vector) UI Image GameObjects, one per page.
 *                            Set all INACTIVE in editor. Order = display order.
 *
 * ── Behaviour ────────────────────────────────────────────────────────────────
 *  - Fire eventName to start.
 *  - BGM starts when the cutscene content starts (after optional black fade-in)
 *    and runs until EndCutscene().
 *  - Each page: image shown → caption set → page audio played → timer starts.
 *  - Timer uses pageDelays[index] if available, else autoAdvanceDelay.
 *  - Left-click skips the timer, stops current page audio, advances early.
 *  - On the last page advancing: page audio stopped, BGM stopped, UI hidden,
 *    "CutsceneDone_<eventName>" fired.
 *
 *  Optional:
 *  - If `switchToSceneOnEnd` is enabled, the controller will call SwitchScene()
 *    after the cutscene finishes (useful for ending cutscene -> credits).
 *
 *  - If `fadeBlackInOnStart` is enabled and `blackFadeInSeconds` > 0, the black
 *    overlay fades from transparent to opaque before BGM and the first page start.
 */
class Cutscene_Controller : public IScript {
public:
    Cutscene_Controller() {
        SCRIPT_FIELD(enableCutscene, Bool);
        SCRIPT_FIELD(eventName, String);
        SCRIPT_FIELD(bgmAudioName, String);
        SCRIPT_FIELD(autoStartOnPlay, Bool);
        SCRIPT_FIELD(autoAdvance, Bool);
        SCRIPT_FIELD(autoAdvanceDelay, Float);
        SCRIPT_FIELD(switchToSceneOnEnd, Bool);
        SCRIPT_FIELD(endScenePath, String);
        SCRIPT_FIELD(fadeBlackInOnStart, Bool);
        SCRIPT_FIELD(blackFadeInSeconds, Float);
        SCRIPT_FIELD(fadeInSeconds, Float);
        SCRIPT_FIELD(fadeOutSeconds, Float);
        SCRIPT_FIELD(blackFadeOutSeconds, Float);
        RegisterFloatVectorField("pageDelays", &pageDelays);
        RegisterStringVectorField("pageAudioNames", &pageAudioNames);
        RegisterStringVectorField("pageCaptions", &pageCaptions);
        SCRIPT_GAMEOBJECT_REF(captionTextRef);
        SCRIPT_GAMEOBJECT_REF(blackBackgroundRef);
        RegisterGameObjectRefVectorField("pageImages", &pageImages);
    }

    ~Cutscene_Controller() override = default;

    void Awake()            override {}
    void Initialize(Entity) override {}
    void OnDestroy()        override {
        StopCurrentPageAudio();
        StopBGM();
        HideAllPages();
        SetCaptionVisible(false);
        ShowBlackBackground(false);
        isPlaying = false;
        blackFadeOutActive = false;
        blackFadeOutTimer = 0.0f;
        blackFadeInActive = false;
        blackFadeInTimer = 0.0f;
        m_pendingStart = false;
    }
    void OnEnable()         override {}
    void OnDisable()        override {}
    void OnValidate()       override {}
    const char* GetTypeName() const override { return "Cutscene_Controller"; }
    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

    void Start() override {
        if (!enableCutscene) {
            HideAllPages();
            SetCaptionVisible(false);
            ShowBlackBackground(false);
            isPlaying = false;
            LOG_INFO("Cutscene_Controller: disabled via enableCutscene=false");
            return;
        }

        // ── Failsafe ──────────────────────────────────────────────────────
        if (eventName.empty() || eventName == "emptyEvent") {
            LOG_ERROR("Cutscene_Controller: eventName is not set! "
                "Assign a real event name in the inspector.");
            eventName = "emptyEvent";
        }

        // ── Validate ──────────────────────────────────────────────────────
        if (pageImages.empty())
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageImages is empty — cutscene has no pages!");

        if (pageCaptions.size() < pageImages.size())
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageCaptions has fewer entries than pageImages. "
                "Missing captions will be blank.");

        if (pageAudioNames.size() < pageImages.size())
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageAudioNames has fewer entries than pageImages. "
                "Missing entries will be silent.");

        if (pageDelays.size() < pageImages.size())
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageDelays has fewer entries than pageImages. "
                "Missing entries will use autoAdvanceDelay ("
                << autoAdvanceDelay << "s).");

        if (autoAdvanceDelay <= 0.0f)
            autoAdvanceDelay = 2.0f;

        // ── Register event listener ───────────────────────────────────────
        LOG_ERROR("listening to " << eventName.c_str());


        Events::Listen(eventName.c_str(), [this](void*) {
            m_pendingStart = true;
            LOG_ERROR(eventName.c_str() << " has been heard!!!");

        });

        HideAllPages();
        SetCaptionVisible(false);

        isPlaying = false;
        currentPageIndex = 0;
        ignoreNextClick = false;
        pageTimer = 0.0f;
        currentPageAudio = "";

        // Initial overlay: full black (legacy) or transparent if we fade in when cutscene starts.
        if (fadeBlackInOnStart && blackBackgroundRef.IsValid() && blackFadeInSeconds > 0.0f) {
            ShowBlackBackgroundVisibleWithAlpha(0.0f);
        } else {
            ShowBlackBackground(true);
        }

        LOG_DEBUG("Cutscene_Controller [" << eventName << "]: Ready with "
            << pageImages.size() << " page(s).");

        if (autoStartOnPlay) {
            // Start on next Update so all UI refs are fully initialized, without a deferred this-callback.
            m_pendingStart = true;
        }
    }

    void Update(double deltaTime) override {
        if (!enableCutscene) return;

        const float dt = static_cast<float>(deltaTime);

        if (m_pendingStart && !isPlaying && !blackFadeInActive) {
            m_pendingStart = false;
            TriggerCutscene();
        }

        if (blackFadeInActive) {
            blackFadeInTimer += dt;
            const float dur = blackFadeInSeconds > 0.0f ? blackFadeInSeconds : 0.0f;
            const float t = (dur <= 0.0f)
                ? 1.0f
                : std::min(1.0f, blackFadeInTimer / dur);
            SetBlackBackgroundAlpha(t);
            if (t >= 1.0f - 1e-4f) {
                blackFadeInActive = false;
                blackFadeInTimer = 0.0f;
                StartCutsceneContent();
            }
        }

        if (blackFadeOutActive) {
            blackFadeOutTimer += dt;
            const float dur = blackFadeOutSeconds > 0.0f ? blackFadeOutSeconds : 0.0f;
            const float t = (dur <= 0.0f) ? 0.0f : std::max(0.0f, 1.0f - (blackFadeOutTimer / dur));
            SetBlackBackgroundAlpha(t);
            if (t <= 0.0f + 1e-4f) {
                ShowBlackBackground(false);
                blackFadeOutActive = false;
                blackFadeOutTimer = 0.0f;
            }
        }

        if (!isPlaying) return;


        // ── Manual left-click advance ─────────────────────────────────────
        if (Input::WasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (ignoreNextClick) {
                ignoreNextClick = false;
            }
            else {
                RequestAdvance();
            }
        }

        UpdateFade(dt);
    }

private:
    // ── Inspector ─────────────────────────────────────────────────────────
    bool                       enableCutscene = true;
    std::string                eventName = "emptyEvent";
    std::string                bgmAudioName = "";
    bool                       autoStartOnPlay = false;
    bool                       autoAdvance = false;
    float                      autoAdvanceDelay = 2.0f;
    bool                       switchToSceneOnEnd = false;
    std::string                endScenePath = ""; // e.g. Credits scene UUID
    bool                       fadeBlackInOnStart = false;
    float                      blackFadeInSeconds = 0.5f;
    float                      fadeInSeconds = 0.18f;
    float                      fadeOutSeconds = 0.18f;
    float                      blackFadeOutSeconds = 0.5f;
    std::vector<float>         pageDelays;        // per-page duration (seconds)
    std::vector<std::string>   pageAudioNames;    // per-page FMOD event path
    std::vector<std::string>   pageCaptions;
    GameObjectRef              captionTextRef;
    GameObjectRef              blackBackgroundRef; // UICanvas or UIImage (black)
    std::vector<GameObjectRef> pageImages;

    // ── Runtime ───────────────────────────────────────────────────────────
    bool        isPlaying = false;
    bool        m_pendingStart = false;
    int         currentPageIndex = 0;
    bool        ignoreNextClick = false;
    float       pageTimer = 0.0f;
    std::string currentPageAudio = ""; // track what's playing so we can stop it

    enum class Phase { FadeIn, Hold, FadeOut };
    Phase phase = Phase::FadeIn;
    float phaseTimer = 0.0f;
    bool  blackFadeOutActive = false;
    float blackFadeOutTimer = 0.0f;
    bool  blackFadeInActive = false;
    float blackFadeInTimer = 0.0f;


    // ── Helpers ───────────────────────────────────────────────────────────

    /** Returns the display duration for a given page index.
     *  Uses pageDelays[index] if available, falls back to autoAdvanceDelay. */
    float GetDelayForPage(int index) const {
        if (index >= 0 && index < static_cast<int>(pageDelays.size())
            && pageDelays[index] > 0.0f) {
            return pageDelays[index];
        }
        return autoAdvanceDelay;
    }

    /** Returns the audio event path for a given page, or "" if none. */
    std::string GetAudioForPage(int index) const {
        if (index >= 0 && index < static_cast<int>(pageAudioNames.size()))
            return pageAudioNames[index];
        return "";
    }

    void TriggerCutscene() {
        if (!enableCutscene) {
            LOG_DEBUG("Cutscene_Controller [" << eventName << "]: start ignored (disabled).");
            return;
        }

        if (pageImages.empty()) {
            LOG_ERROR("Cutscene_Controller [" << eventName
                << "]: Cannot start — pageImages is empty!");
            return;
        }

        LOG_INFO("Cutscene_Controller [" << eventName << "]: Starting.");

        if (fadeBlackInOnStart && blackFadeInSeconds > 0.0f && blackBackgroundRef.IsValid()) {
            blackFadeOutActive = false;
            blackFadeOutTimer = 0.0f;
            ShowBlackBackgroundVisibleWithAlpha(0.0f);
            blackFadeInActive = true;
            blackFadeInTimer = 0.0f;
            return;
        }

        StartCutsceneContent();
    }

    /** BGM + first page; runs after optional black overlay fade-in. */
    void StartCutsceneContent() {
        isPlaying = true;
        currentPageIndex = 0;
        ignoreNextClick = true;
        pageTimer = 0.0f;
        currentPageAudio = "";
        phase = Phase::FadeIn;
        phaseTimer = 0.0f;

        PlayBGM();
        ShowBlackBackground(true);
        ShowPage(currentPageIndex);
    }

    void AdvancePage() {
        // Stop the current page's audio before leaving
        StopCurrentPageAudio();
        HidePage(currentPageIndex);

        ++currentPageIndex;

        if (currentPageIndex < static_cast<int>(pageImages.size())) {
            ShowPage(currentPageIndex);
            phase = Phase::FadeIn;
            phaseTimer = 0.0f;
            pageTimer = 0.0f;
        }
        else {
            EndCutscene();
        }
    }

    void ShowPage(int index) {
        if (index < 0 || index >= static_cast<int>(pageImages.size())) return;

        // Show image
        if (pageImages[index].IsValid()) {
            SetActive(true, pageImages[index].GetEntity());
            SetPageImageAlpha(index, 0.0f);
        }
        else {
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageImages[" << index << "] is not a valid reference!");
        }

        // Caption
        const std::string& caption = (index < static_cast<int>(pageCaptions.size()))
            ? pageCaptions[index] : "";
        UpdateCaption(caption);
        SetCaptionAlpha(0.0f);

        // Per-page audio
        std::string audio = GetAudioForPage(index);
        if (!audio.empty()) {
            PlayAudio("event:/" + audio);
            currentPageAudio = audio;
        }
        else {
            currentPageAudio = "";
        }
    }

    void HidePage(int index) {
        if (index < 0 || index >= static_cast<int>(pageImages.size())) return;
        if (pageImages[index].IsValid())
            SetActive(false, pageImages[index].GetEntity());
    }

    void HideAllPages() {
        for (auto& page : pageImages)
            if (page.IsValid())
                SetActive(false, page.GetEntity());
    }

    void ShowBlackBackground(bool visible) {
        if (!blackBackgroundRef.IsValid())
            return;

        const Entity e = blackBackgroundRef.GetEntity();
        SetActive(visible, e);
        if (!visible)
            return;

        // Force opaque black. Supports either a background UICanvas or a UIImage.
        if (NE::ECS::Query::HasUICanvas(e)) {
            NE::ECS::Command::SetUICanvasAlpha(e, 1.0f);
        }
        else if (NE::ECS::Query::HasUIImage(e)) {
            NE::ECS::Command::SetUIImageColor(e, 0.0f, 0.0f, 0.0f, 1.0f);
        }
        else {
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: blackBackgroundRef has no UICanvas/UIImage.");
        }
    }

    /** Show overlay active at a specific alpha (for fade-in from transparent). */
    void ShowBlackBackgroundVisibleWithAlpha(float a) {
        if (!blackBackgroundRef.IsValid())
            return;
        const Entity e = blackBackgroundRef.GetEntity();
        SetActive(true, e);
        if (!NE::ECS::Query::HasUICanvas(e) && !NE::ECS::Query::HasUIImage(e)) {
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: blackBackgroundRef has no UICanvas/UIImage.");
            return;
        }
        SetBlackBackgroundAlpha(a);
    }

    void SetBlackBackgroundAlpha(float a) {
        if (!blackBackgroundRef.IsValid())
            return;
        const Entity e = blackBackgroundRef.GetEntity();
        const float alpha = std::clamp(a, 0.0f, 1.0f);
        if (NE::ECS::Query::HasUICanvas(e)) {
            NE::ECS::Command::SetUICanvasAlpha(e, alpha);
        }
        else if (NE::ECS::Query::HasUIImage(e)) {
            NE::ECS::Command::SetUIImageColor(e, 0.0f, 0.0f, 0.0f, alpha);
        }
    }

    void SetCaptionVisible(bool visible) {
        if (captionTextRef.IsValid())
            SetActive(visible, captionTextRef.GetEntity());
    }

    void UpdateCaption(const std::string& text) {
        if (!captionTextRef.IsValid()) return;
        if (text.empty()) {
            SetCaptionVisible(false);
            return;
        }
        NE::Scripting::SetUIText(captionTextRef.GetEntity(), text.c_str());
        SetCaptionVisible(true);
    }

    void SetCaptionAlpha(float a) {
        if (!captionTextRef.IsValid())
            return;
        const Entity e = captionTextRef.GetEntity();
        if (!NE::ECS::Query::HasUIText(e))
            return;
        NE::ECS::Command::SetUITextColor(e, 1.0f, 1.0f, 1.0f, std::clamp(a, 0.0f, 1.0f));
    }

    void SetPageImageAlpha(int index, float a) {
        if (index < 0 || index >= static_cast<int>(pageImages.size()))
            return;
        if (!pageImages[index].IsValid())
            return;
        const Entity e = pageImages[index].GetEntity();
        if (!NE::ECS::Query::HasUIImage(e))
            return;
        NE::ECS::Command::SetUIImageColor(e, 1.0f, 1.0f, 1.0f, std::clamp(a, 0.0f, 1.0f));
    }

    void RequestAdvance() {
        if (!isPlaying) return;
        if (phase == Phase::FadeOut) return;
        phase = Phase::FadeOut;
        phaseTimer = 0.0f;
        pageTimer = 0.0f;
    }

    void UpdateFade(float dt) {
        // Clamp to safe values.
        if (fadeInSeconds < 0.0f) fadeInSeconds = 0.0f;
        if (fadeOutSeconds < 0.0f) fadeOutSeconds = 0.0f;

        phaseTimer += dt;

        switch (phase) {
        case Phase::FadeIn: {
            const float dur = fadeInSeconds;
            const float t = (dur <= 0.0f) ? 1.0f : std::min(1.0f, phaseTimer / dur);
            SetPageImageAlpha(currentPageIndex, t);
            if (captionTextRef.IsValid())
                SetCaptionAlpha(t);
            if (t >= 1.0f - 1e-4f) {
                phase = Phase::Hold;
                phaseTimer = 0.0f;
                pageTimer = 0.0f;
            }
            break;
        }
        case Phase::Hold: {
            // Auto-advance timer
            if (autoAdvance) {
                pageTimer += dt;
                const float delay = GetDelayForPage(currentPageIndex);
                if (pageTimer >= delay) {
                    phase = Phase::FadeOut;
                    phaseTimer = 0.0f;
                    pageTimer = 0.0f;
                }
            }
            break;
        }
        case Phase::FadeOut: {
            const float dur = fadeOutSeconds;
            const float t = (dur <= 0.0f) ? 0.0f : std::max(0.0f, 1.0f - (phaseTimer / dur));
            SetPageImageAlpha(currentPageIndex, t);
            if (captionTextRef.IsValid())
                SetCaptionAlpha(t);
            if (t <= 0.0f + 1e-4f) {
                SetPageImageAlpha(currentPageIndex, 0.0f);
                SetCaptionAlpha(0.0f);
                AdvancePage();
            }
            break;
        }
        }
    }

    void PlayBGM() {
        if (!bgmAudioName.empty())
            PlayAudio("event:/" + bgmAudioName);
    }

    void StopBGM() {
        if (!bgmAudioName.empty())
            StopAudio("event:/" + bgmAudioName);
    }

    /** Stops whatever per-page audio is currently playing, if any. */
    void StopCurrentPageAudio() {
        if (!currentPageAudio.empty()) {
            StopAudio("event:/" + currentPageAudio);
            currentPageAudio = "";
        }
    }

    void EndCutscene() {
        isPlaying = false;
        HideAllPages();
        SetCaptionVisible(false);
        StopBGM();
        blackFadeOutActive = true;
        blackFadeOutTimer = 0.0f;
        SetBlackBackgroundAlpha(1.0f);

        std::string doneEvent = "CutsceneDone_" + eventName;
        Events::Send(doneEvent.c_str());

        LOG_INFO("Cutscene_Controller [" << eventName << "]: Finished. "
            "Fired event: " << doneEvent);

        if (switchToSceneOnEnd && !endScenePath.empty()) {
            NE::Scripting::SwitchScene(endScenePath);
        }
    }
};