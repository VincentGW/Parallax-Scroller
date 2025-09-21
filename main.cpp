#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <map>
#include <algorithm>

const int TILE_SIZE = 256;
const int STRIP_HEIGHT = 128;
const int SMALL_STRIP_HEIGHT = 64;
const int STRIPS_PER_TILE = TILE_SIZE / STRIP_HEIGHT; // 2 strips per tile for above/below
const int SMALL_STRIPS_PER_TILE = TILE_SIZE / SMALL_STRIP_HEIGHT; // 4 strips per tile for top/bottom
const int TILES_ABOVE = 2; // a2 above a1
const int TILES_BELOW = 2; // b1 above b2
const int ZENITH_HEIGHT = TILE_SIZE; // Zenith is one full tile (256px)

// These will be set in main() based on desktop resolution
int SCREEN_WIDTH;
int SCREEN_HEIGHT;
int ZENITH_START_Y;
int ZENITH_END_Y;
const float BASE_SCROLL_SPEED = 50.0f;
const float SCROLL_SPEED_COEFFICIENT = 0.5f; // Controls how much faster each layer gets

class ParallaxLayer {
private:
    // Each tile can have multiple animation frames
    std::vector<std::vector<sf::Texture>> tileAnimations; // [tileIndex][frameIndex]
    std::vector<std::vector<sf::Sprite>> tileSprites;     // [tileIndex][frameIndex]
    std::vector<int> currentFrames;  // Current frame for each tile
    std::vector<float> animationTimers; // Timer for each tile's animation
    float scrollOffset;
    float scrollSpeed;
    int yPosition;
    int height;
    sf::IntRect sourceRect;
    bool hasTextures;

public:
    ParallaxLayer(float speed, int y, int h)
        : scrollOffset(0), scrollSpeed(speed), yPosition(y), height(h), hasTextures(false) {
        sourceRect = sf::IntRect(0, 0, TILE_SIZE, height);
    }

    bool loadTextures(const std::vector<std::vector<std::string>>& tileFrameFiles, int stripIndex = -1) {
        tileAnimations.clear();
        tileSprites.clear();
        currentFrames.clear();
        animationTimers.clear();

        for (const auto& frameFiles : tileFrameFiles) {
            std::vector<sf::Texture> frameTextures;
            std::vector<sf::Sprite> frameSprites;

            for (const auto& filename : frameFiles) {
                sf::Texture texture;
                if (!texture.loadFromFile(filename)) {
                    std::cerr << "Failed to load texture: " << filename << std::endl;
                    continue;
                }

                // Set up source rectangle for strips
                if (stripIndex >= 0) {
                    // This is a strip from a larger tile - use the layer's height to determine strip size
                    int stripHeight = height; // Use the layer's height (could be 64px or 128px)
                    sourceRect = sf::IntRect(0, stripIndex * stripHeight, TILE_SIZE, stripHeight);
                } else {
                    // This is the full zenith tile - use full tile dimensions
                    sourceRect = sf::IntRect(0, 0, TILE_SIZE, TILE_SIZE);
                }

                frameTextures.push_back(std::move(texture));

                sf::Sprite sprite;
                sprite.setTexture(frameTextures.back());
                sprite.setTextureRect(sourceRect);
                sprite.setPosition(0, (float)yPosition);
                sprite.setScale(1.0f, 1.0f);

                frameSprites.push_back(sprite);

                std::cout << "Loaded frame: " << filename;
                if (stripIndex >= 0) {
                    std::cout << " (strip " << stripIndex << ")";
                }
                std::cout << std::endl;
            }

            if (!frameTextures.empty()) {
                // Fix sprite texture references
                for (size_t i = 0; i < frameSprites.size(); i++) {
                    frameSprites[i].setTexture(frameTextures[i]);
                }

                tileAnimations.push_back(std::move(frameTextures));
                tileSprites.push_back(std::move(frameSprites));
                currentFrames.push_back(0); // Start at frame 0
                animationTimers.push_back(0.0f); // Start timer at 0
            }
        }

        if (!tileAnimations.empty()) {
            hasTextures = true;
            return true;
        }

        return false;
    }

    void update(float deltaTime, bool scrollLeft, bool scrollRight) {
        if (scrollLeft) {
            scrollOffset -= scrollSpeed * deltaTime;
        }
        if (scrollRight) {
            scrollOffset += scrollSpeed * deltaTime;
        }

        // Keep offset within pattern bounds for seamless wrapping
        if (!tileAnimations.empty()) {
            int patternWidth = tileAnimations.size() * TILE_SIZE;
            while (scrollOffset > patternWidth) scrollOffset -= patternWidth;
            while (scrollOffset < -patternWidth) scrollOffset += patternWidth;
        }

        // Update animations for each tile
        for (size_t tileIndex = 0; tileIndex < tileAnimations.size(); tileIndex++) {
            if (tileAnimations[tileIndex].size() <= 1) continue; // Skip single frame tiles

            animationTimers[tileIndex] += deltaTime;

            // Check if current frame is frame 0 (hold for 3 seconds)
            if (currentFrames[tileIndex] == 0) {
                if (animationTimers[tileIndex] >= 3.0f) {
                    // Move to next frame after 3 seconds
                    currentFrames[tileIndex] = 1;
                    animationTimers[tileIndex] = 0.0f;
                }
            } else {
                // Normal frame timing (0.2 seconds per frame for smooth animation)
                if (animationTimers[tileIndex] >= 0.2f) {
                    currentFrames[tileIndex]++;
                    animationTimers[tileIndex] = 0.0f;

                    // Loop back to frame 0 when we reach the end
                    if (currentFrames[tileIndex] >= static_cast<int>(tileAnimations[tileIndex].size())) {
                        currentFrames[tileIndex] = 0;
                    }
                }
            }
        }
    }

    void render(sf::RenderWindow& window) {
        if (!hasTextures || tileAnimations.empty()) {
            // Skip rendering if no textures loaded
            return;
        }

        int patternLength = tileAnimations.size();
        int patternWidth = patternLength * TILE_SIZE; // Total width of one complete pattern

        // Calculate how many complete patterns we need to cover the screen
        int patternsNeeded = (SCREEN_WIDTH / patternWidth) + 3; // Extra patterns for smooth scrolling

        for (int patternIndex = -1; patternIndex <= patternsNeeded; patternIndex++) {
            for (int tileInPattern = 0; tileInPattern < patternLength; tileInPattern++) {
                float xPos = (patternIndex * patternWidth) + (tileInPattern * TILE_SIZE) + scrollOffset;

                // Only draw if visible on screen
                if (xPos + TILE_SIZE >= 0 && xPos <= SCREEN_WIDTH) {
                    // Get the current frame for this tile
                    int currentFrame = currentFrames[tileInPattern];
                    if (currentFrame < static_cast<int>(tileSprites[tileInPattern].size())) {
                        sf::Sprite renderSprite = tileSprites[tileInPattern][currentFrame];
                        renderSprite.setPosition(xPos, (float)yPosition);
                        window.draw(renderSprite);
                    }
                }
            }
        }
    }

    float getScrollSpeed() const { return scrollSpeed; }
    int getYPosition() const { return yPosition; }
    bool getHasTextures() const { return hasTextures; }
};

class ParallaxScroller {
private:
    std::vector<ParallaxLayer> layers;
    sf::RenderWindow& window;

    // Auto-scroll state
    enum class AutoScrollState {
        STOPPED,
        SCROLLING_RIGHT,
        SCROLLING_LEFT
    };
    AutoScrollState autoScrollState;
    int autoScrollCycle; // 0=stop, 1=right, 2=stop, 3=left (cycles 0-3)

public:
    ParallaxScroller(sf::RenderWindow& win) : window(win), autoScrollState(AutoScrollState::STOPPED), autoScrollCycle(0) {
        initializeLayers();
        loadTiles();
    }

    void initializeLayers() {
        layers.clear();

        // Create all available strips from tiles
        int aboveStrips = STRIPS_PER_TILE; // 2 strips of 128px each
        int topStrips = SMALL_STRIPS_PER_TILE; // 4 strips of 64px each
        int belowStrips = STRIPS_PER_TILE; // 2 strips of 128px each
        int bottomStrips = SMALL_STRIPS_PER_TILE; // 4 strips of 64px each

        // Above strips (128px each) - start from just above zenith
        for (int i = 0; i < aboveStrips; i++) {
            int y = ZENITH_START_Y - (i + 1) * STRIP_HEIGHT;
            float speedMultiplier = 1.0f + (i + 1) * SCROLL_SPEED_COEFFICIENT;
            ParallaxLayer layer(BASE_SCROLL_SPEED * speedMultiplier, y, STRIP_HEIGHT);
            layers.push_back(layer);
        }

        // Top strips (64px each) - start from above the "above" strips
        for (int i = 0; i < topStrips; i++) {
            int y = ZENITH_START_Y - (aboveStrips * STRIP_HEIGHT) - (i + 1) * SMALL_STRIP_HEIGHT;
            float speedMultiplier = 1.0f + (aboveStrips + i + 1) * SCROLL_SPEED_COEFFICIENT; // Continue speed progression
            ParallaxLayer layer(BASE_SCROLL_SPEED * speedMultiplier, y, SMALL_STRIP_HEIGHT);
            layers.push_back(layer);
        }

        // Zenith layer (mountains) - centered, base speed
        ParallaxLayer zenithLayer(BASE_SCROLL_SPEED, ZENITH_START_Y, ZENITH_HEIGHT);
        layers.push_back(zenithLayer);

        // Below strips (128px each) - start from just below zenith
        for (int i = 0; i < belowStrips; i++) {
            int y = ZENITH_END_Y + i * STRIP_HEIGHT;
            float speedMultiplier = 1.0f + (i + 1) * SCROLL_SPEED_COEFFICIENT;
            ParallaxLayer layer(BASE_SCROLL_SPEED * speedMultiplier, y, STRIP_HEIGHT);
            layers.push_back(layer);
        }

        // Bottom strips (64px each) - start from below the "below" strips
        for (int i = 0; i < bottomStrips; i++) {
            int y = ZENITH_END_Y + (belowStrips * STRIP_HEIGHT) + i * SMALL_STRIP_HEIGHT;
            float speedMultiplier = 1.0f + (belowStrips + i + 1) * SCROLL_SPEED_COEFFICIENT; // Continue speed progression
            ParallaxLayer layer(BASE_SCROLL_SPEED * speedMultiplier, y, SMALL_STRIP_HEIGHT);
            layers.push_back(layer);
        }

        std::cout << "Created " << layers.size() << " parallax layers:" << std::endl;
        std::cout << "- " << topStrips << " top strips (64px each)" << std::endl;
        std::cout << "- " << aboveStrips << " above strips (128px each)" << std::endl;
        std::cout << "- 1 zenith layer centered at y=" << ZENITH_START_Y << std::endl;
        std::cout << "- " << belowStrips << " below strips (128px each)" << std::endl;
        std::cout << "- " << bottomStrips << " bottom strips (64px each)" << std::endl;
        std::cout << "- Screen resolution: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << " pixels" << std::endl;
    }

    void loadTiles() {
        std::cout << "\n=== Loading Tiles ===" << std::endl;

        // Load tile animations from directories
        std::vector<std::vector<std::string>> zenithAnimations = getTileSequencesInDirectory("assets/tiles/zenith");
        std::vector<std::vector<std::string>> topAnimations = getTileSequencesInDirectory("assets/tiles/top");
        std::vector<std::vector<std::string>> aboveAnimations = getTileSequencesInDirectory("assets/tiles/above");
        std::vector<std::vector<std::string>> belowAnimations = getTileSequencesInDirectory("assets/tiles/below");
        std::vector<std::vector<std::string>> bottomAnimations = getTileSequencesInDirectory("assets/tiles/bottom");

        int layerIndex = 0;

        // Load above tiles first (128px strips, directly above zenith)
        if (!aboveAnimations.empty()) {
            for (int i = 0; i < STRIPS_PER_TILE; i++) {
                if (layerIndex < layers.size()) {
                    layers[layerIndex].loadTextures(aboveAnimations, STRIPS_PER_TILE - 1 - i); // above, strips in reverse order
                }
                layerIndex++;
            }
        }

        // Load top tiles second (64px strips, above "above")
        if (!topAnimations.empty()) {
            for (int i = 0; i < SMALL_STRIPS_PER_TILE; i++) {
                if (layerIndex < layers.size()) {
                    layers[layerIndex].loadTextures(topAnimations, SMALL_STRIPS_PER_TILE - 1 - i); // top, strips in reverse order
                }
                layerIndex++;
            }
        }

        // Load zenith layer (full tile)
        if (!zenithAnimations.empty() && layerIndex < layers.size()) {
            layers[layerIndex].loadTextures(zenithAnimations); // Full zenith tiles
        }
        layerIndex++;

        // Load below tiles first (128px strips, directly under zenith)
        if (!belowAnimations.empty()) {
            for (int i = 0; i < STRIPS_PER_TILE; i++) {
                if (layerIndex < layers.size()) {
                    layers[layerIndex].loadTextures(belowAnimations, i); // below, strips in normal order
                }
                layerIndex++;
            }
        }

        // Load bottom tiles second (64px strips, below "below")
        if (!bottomAnimations.empty()) {
            for (int i = 0; i < SMALL_STRIPS_PER_TILE; i++) {
                if (layerIndex < layers.size()) {
                    layers[layerIndex].loadTextures(bottomAnimations, i); // bottom, strips in normal order
                }
                layerIndex++;
            }
        }

        std::cout << "=== Tile Loading Complete ===" << std::endl;
    }

    std::vector<std::vector<std::string>> getTileSequencesInDirectory(const std::string& directory) {
        std::vector<std::vector<std::string>> tileAnimations;
        std::map<std::string, std::vector<std::string>> subfolderFrames;

        try {
            if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory)) {
                // First, check for subfolders (a, b, c, d, etc.)
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_directory()) {
                        std::string subfolderName = entry.path().filename().string();
                        if (subfolderName.length() == 1 && subfolderName >= "a" && subfolderName <= "z") {
                            std::vector<std::string> frameFiles = getAnimationFrames(entry.path().string());
                            if (!frameFiles.empty()) {
                                subfolderFrames[subfolderName] = frameFiles;
                            }
                        }
                    }
                }

                // If we found subfolders, create tile animations from them (a, b, c, d order)
                if (!subfolderFrames.empty()) {
                    for (char c = 'a'; c <= 'z'; c++) {
                        std::string key(1, c);
                        if (subfolderFrames.find(key) != subfolderFrames.end()) {
                            tileAnimations.push_back(subfolderFrames[key]);
                        }
                    }
                } else {
                    // Fallback: treat direct files as a single tile with single frame
                    std::vector<std::string> directFiles = getFilesInDirectory(directory);
                    if (!directFiles.empty()) {
                        // Each file becomes a single-frame tile
                        for (const auto& file : directFiles) {
                            tileAnimations.push_back({file});
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Filesystem error: " << ex.what() << std::endl;
        }
        return tileAnimations;
    }

    std::vector<std::string> getFilesInDirectory(const std::string& directory) {
        std::vector<std::string> files;
        try {
            if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory)) {
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_regular_file()) {
                        std::string extension = entry.path().extension().string();
                        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp") {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Filesystem error: " << ex.what() << std::endl;
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // Get animation frames for a single tile (all numbered files in a subfolder)
    std::vector<std::string> getAnimationFrames(const std::string& tileDirectory) {
        std::vector<std::string> frameFiles;
        std::map<int, std::string> frameMap;

        try {
            if (std::filesystem::exists(tileDirectory) && std::filesystem::is_directory(tileDirectory)) {
                for (const auto& entry : std::filesystem::directory_iterator(tileDirectory)) {
                    if (entry.is_regular_file()) {
                        std::string extension = entry.path().extension().string();
                        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp") {
                            std::string filename = entry.path().stem().string();
                            try {
                                int frameNumber = std::stoi(filename);
                                frameMap[frameNumber] = entry.path().string();
                            } catch (const std::exception&) {
                                // If filename is not a number, skip it
                                continue;
                            }
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Filesystem error: " << ex.what() << std::endl;
        }

        // Convert map to vector in correct order (0, 1, 2, 3...)
        for (const auto& pair : frameMap) {
            frameFiles.push_back(pair.second);
        }

        return frameFiles;
    }

    void reloadTiles() {
        std::cout << "\n=== Reloading Tiles ===" << std::endl;
        loadTiles();
    }

    void toggleAutoScroll() {
        autoScrollCycle = (autoScrollCycle + 1) % 4;

        switch (autoScrollCycle) {
            case 0:
                autoScrollState = AutoScrollState::STOPPED;
                std::cout << "Auto-scroll: STOPPED" << std::endl;
                break;
            case 1:
                autoScrollState = AutoScrollState::SCROLLING_RIGHT;
                std::cout << "Auto-scroll: SCROLLING RIGHT" << std::endl;
                break;
            case 2:
                autoScrollState = AutoScrollState::STOPPED;
                std::cout << "Auto-scroll: STOPPED" << std::endl;
                break;
            case 3:
                autoScrollState = AutoScrollState::SCROLLING_LEFT;
                std::cout << "Auto-scroll: SCROLLING LEFT" << std::endl;
                break;
        }
    }

    void update(float deltaTime, bool scrollLeft, bool scrollRight) {
        // Override manual scroll input with auto-scroll state
        bool finalScrollLeft = scrollLeft;
        bool finalScrollRight = scrollRight;

        switch (autoScrollState) {
            case AutoScrollState::SCROLLING_RIGHT:
                finalScrollLeft = false;
                finalScrollRight = true;
                break;
            case AutoScrollState::SCROLLING_LEFT:
                finalScrollLeft = true;
                finalScrollRight = false;
                break;
            case AutoScrollState::STOPPED:
                // Manual input is preserved when auto-scroll is stopped
                break;
        }

        for (auto& layer : layers) {
            layer.update(deltaTime, finalScrollLeft, finalScrollRight);
        }
    }

    void render() {
        // Clear with black background (no visible gaps)
        window.clear(sf::Color::Black);

        // Render all layers back to front
        for (auto& layer : layers) {
            layer.render(window);
        }

        // Debug overlay removed for clean visual
    }

    void renderDebugOverlay() {
        // Draw zenith region boundaries
        sf::RectangleShape zenithTop(sf::Vector2f((float)SCREEN_WIDTH, 2));
        zenithTop.setPosition(0, (float)ZENITH_START_Y);
        zenithTop.setFillColor(sf::Color(255, 255, 0, 180)); // Yellow
        window.draw(zenithTop);

        sf::RectangleShape zenithBottom(sf::Vector2f((float)SCREEN_WIDTH, 2));
        zenithBottom.setPosition(0, (float)ZENITH_END_Y);
        zenithBottom.setFillColor(sf::Color(255, 255, 0, 180)); // Yellow
        window.draw(zenithBottom);

        // Draw strip boundaries
        for (int i = 1; i < 4; i++) {
            // Above zenith
            int yAbove = ZENITH_START_Y - i * STRIP_HEIGHT;
            if (yAbove >= 0) {
                sf::RectangleShape stripLine(sf::Vector2f((float)SCREEN_WIDTH, 1));
                stripLine.setPosition(0, (float)yAbove);
                stripLine.setFillColor(sf::Color(255, 0, 255, 100)); // Magenta
                window.draw(stripLine);
            }

            // Below zenith
            int yBelow = ZENITH_END_Y + i * STRIP_HEIGHT;
            if (yBelow < SCREEN_HEIGHT) {
                sf::RectangleShape stripLine(sf::Vector2f((float)SCREEN_WIDTH, 1));
                stripLine.setPosition(0, (float)yBelow);
                stripLine.setFillColor(sf::Color(0, 255, 255, 100)); // Cyan
                window.draw(stripLine);
            }
        }
    }
};

int main() {
    // Get desktop resolution for full screen windowed mode
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    SCREEN_WIDTH = desktop.width;
    SCREEN_HEIGHT = desktop.height;

    // Center the zenith tile vertically on screen
    ZENITH_START_Y = (SCREEN_HEIGHT - ZENITH_HEIGHT) / 2;
    ZENITH_END_Y = ZENITH_START_Y + ZENITH_HEIGHT;

    sf::RenderWindow window(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), "SEGA Genesis Style Parallax Scroller - SFML", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    ParallaxScroller scroller(window);

    std::cout << "\n=== PARALLAX SCROLLER CONTROLS ===" << std::endl;
    std::cout << "Left Arrow / A - Scroll left" << std::endl;
    std::cout << "Right Arrow / D - Scroll right" << std::endl;
    std::cout << "SPACE - Toggle auto-scroll (stop -> right -> stop -> left -> stop...)" << std::endl;
    std::cout << "R - Reload tiles" << std::endl;
    std::cout << "ESC - Exit" << std::endl;
    std::cout << "\n=== TILE SYSTEM ===" << std::endl;
    std::cout << "Place 256x256 tiles in:" << std::endl;
    std::cout << "- assets/tiles/zenith/  (center region)" << std::endl;
    std::cout << "- assets/tiles/above/   (directly above zenith, splits into strips)" << std::endl;
    std::cout << "- assets/tiles/top/     (furthest from zenith, splits into strips)" << std::endl;
    std::cout << "- assets/tiles/below/   (directly below zenith, splits into strips)" << std::endl;
    std::cout << "- assets/tiles/bottom/  (furthest from zenith, splits into strips)" << std::endl;

    sf::Clock clock;

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::R) {
                    scroller.reloadTiles();
                }
                if (event.key.code == sf::Keyboard::Space) {
                    scroller.toggleAutoScroll();
                }
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                }
            }
        }

        // Handle continuous key input
        bool scrollLeft = sf::Keyboard::isKeyPressed(sf::Keyboard::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::D);
        bool scrollRight = sf::Keyboard::isKeyPressed(sf::Keyboard::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::A);

        // Update
        scroller.update(deltaTime, scrollLeft, scrollRight);

        // Render
        scroller.render();
        window.display();
    }

    return 0;
}