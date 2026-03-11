#pragma once
#include "EngineAPI.hpp"

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
 *  - BGM starts immediately and runs until EndCutscene().
 *  - Each page: image shown → caption set → page audio played → timer starts.
 *  - Timer uses pageDelays[index] if available, else autoAdvanceDelay.
 *  - Left-click skips the timer, stops current page audio, advances early.
 *  - On the last page advancing: page audio stopped, BGM stopped, UI hidden,
 *    "CutsceneDone_<eventName>" fired.
 */
class Cutscene_Controller : public IScript {
public:
    Cutscene_Controller() {
        SCRIPT_FIELD(eventName, String);
        SCRIPT_FIELD(bgmAudioName, String);
        SCRIPT_FIELD(autoAdvance, Bool);
        SCRIPT_FIELD(autoAdvanceDelay, Float);
        RegisterFloatVectorField("pageDelays", &pageDelays);
        RegisterStringVectorField("pageAudioNames", &pageAudioNames);
        RegisterStringVectorField("pageCaptions", &pageCaptions);
        SCRIPT_GAMEOBJECT_REF(captionTextRef);
        RegisterGameObjectRefVectorField("pageImages", &pageImages);
    }

    ~Cutscene_Controller() override = default;

    void Awake()            override {}
    void Initialize(Entity) override {}
    void OnDestroy()        override {
        if (isPlaying) {
            StopCurrentPageAudio();
            StopBGM();
            HideAllPages();
            SetCaptionVisible(false);
        }
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
        Events::Listen(eventName.c_str(), [this](void*) {
            TriggerCutscene();
            });

        HideAllPages();
        SetCaptionVisible(false);

        isPlaying = false;
        currentPageIndex = 0;
        ignoreNextClick = false;
        pageTimer = 0.0f;
        currentPageAudio = "";

        LOG_DEBUG("Cutscene_Controller [" << eventName << "]: Ready with "
            << pageImages.size() << " page(s).");

        //TriggerCutscene(); // temp here
    }

    void Update(double deltaTime) override {
        if (!isPlaying) return;

        // ── Auto-advance timer ────────────────────────────────────────────
        if (autoAdvance) {
            pageTimer += static_cast<float>(deltaTime);

            float delay = GetDelayForPage(currentPageIndex);
            if (pageTimer >= delay) {
                pageTimer = 0.0f;
                ignoreNextClick = false;
                AdvancePage();
                return;
            }
        }

        // ── Manual left-click advance ─────────────────────────────────────
        if (Input::WasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (ignoreNextClick) {
                ignoreNextClick = false;
                return;
            }
            pageTimer = 0.0f;
            AdvancePage();
        }
    }

private:
    // ── Inspector ─────────────────────────────────────────────────────────
    std::string                eventName = "emptyEvent";
    std::string                bgmAudioName = "";
    bool                       autoAdvance = false;
    float                      autoAdvanceDelay = 2.0f;
    std::vector<float>         pageDelays;        // per-page duration (seconds)
    std::vector<std::string>   pageAudioNames;    // per-page FMOD event path
    std::vector<std::string>   pageCaptions;
    GameObjectRef              captionTextRef;
    std::vector<GameObjectRef> pageImages;

    // ── Runtime ───────────────────────────────────────────────────────────
    bool        isPlaying = false;
    int         currentPageIndex = 0;
    bool        ignoreNextClick = false;
    float       pageTimer = 0.0f;
    std::string currentPageAudio = ""; // track what's playing so we can stop it


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
        if (pageImages.empty()) {
            LOG_ERROR("Cutscene_Controller [" << eventName
                << "]: Cannot start — pageImages is empty!");
            return;
        }

        LOG_INFO("Cutscene_Controller [" << eventName << "]: Starting.");
        isPlaying = true;
        currentPageIndex = 0;
        ignoreNextClick = true;
        pageTimer = 0.0f;
        currentPageAudio = "";

        PlayBGM();
        ShowPage(currentPageIndex);
    }

    void AdvancePage() {
        // Stop the current page's audio before leaving
        StopCurrentPageAudio();
        HidePage(currentPageIndex);

        ++currentPageIndex;

        if (currentPageIndex < static_cast<int>(pageImages.size())) {
            ShowPage(currentPageIndex);
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
        }
        else {
            LOG_WARNING("Cutscene_Controller [" << eventName
                << "]: pageImages[" << index << "] is not a valid reference!");
        }

        // Caption
        const std::string& caption = (index < static_cast<int>(pageCaptions.size()))
            ? pageCaptions[index] : "";
        UpdateCaption(caption);

        // Per-page audio
        std::string audio = GetAudioForPage(index);
        if (!audio.empty()) {
            PlayAudio("event:/"+audio);
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

    void PlayBGM() {
        if (!bgmAudioName.empty())
            PlayAudio("event:/"+bgmAudioName);
    }

    void StopBGM() {
        if (!bgmAudioName.empty())
            StopAudio("event:/"+bgmAudioName);
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

        std::string doneEvent = "CutsceneDone_" + eventName;
        Events::Send(doneEvent.c_str());

        LOG_INFO("Cutscene_Controller [" << eventName << "]: Finished. "
            "Fired event: " << doneEvent);
    }
};