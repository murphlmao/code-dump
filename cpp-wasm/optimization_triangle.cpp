#include <emscripten/emscripten.h>
#include <cmath>
#include <algorithm>

// Optimized: Use simple structs without constructors for better performance
struct Point2D {
    double x, y;
};

struct BarycentricCoord {
    double performance;
    double velocity;
    double adaptability;
};

// Optimized: Inline math functions for speed
inline double distanceSq(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return dx * dx + dy * dy;
}

inline double signedArea(double x1, double y1, double x2, double y2, double x3, double y3) {
    return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
}

// Optimized: Store triangle as simple doubles instead of Point2D objects
static double topX, topY, leftX, leftY, rightX, rightY;
static double invTotalArea; // Pre-calculate inverse for division optimization

// Dot position and state
static BarycentricCoord dotPosition = {0.33, 0.33, 0.34};
static bool isDragging = false;

// Cached dot cartesian position to avoid recalculation
static double cachedDotX, cachedDotY;
static bool dotPositionDirty = true;

// Optimized: Inline barycentric to cartesian conversion
inline void updateDotCartesian() {
    if (dotPositionDirty) {
        cachedDotX = dotPosition.performance * topX +
                     dotPosition.velocity * leftX +
                     dotPosition.adaptability * rightX;
        cachedDotY = dotPosition.performance * topY +
                     dotPosition.velocity * leftY +
                     dotPosition.adaptability * rightY;
        dotPositionDirty = false;
    }
}

// Optimized: Fast point-in-triangle test
inline bool isInsideTriangle(double px, double py) {
    double d1 = signedArea(px, py, topX, topY, leftX, leftY);
    double d2 = signedArea(px, py, leftX, leftY, rightX, rightY);
    double d3 = signedArea(px, py, rightX, rightY, topX, topY);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

// Optimized: Project point onto line segment
inline void projectOntoSegment(double px, double py,
                               double ax, double ay, double bx, double by,
                               double& outX, double& outY) {
    double dx = bx - ax;
    double dy = by - ay;
    double lengthSq = dx * dx + dy * dy;

    if (lengthSq < 0.001) {
        outX = ax;
        outY = ay;
        return;
    }

    double t = ((px - ax) * dx + (py - ay) * dy) / lengthSq;
    t = std::max(0.0, std::min(1.0, t));

    outX = ax + t * dx;
    outY = ay + t * dy;
}

// Optimized: Clamp to triangle boundary
inline void clampToTriangle(double px, double py, double& outX, double& outY) {
    if (isInsideTriangle(px, py)) {
        outX = px;
        outY = py;
        return;
    }

    // Project onto all three edges
    double proj1X, proj1Y, proj2X, proj2Y, proj3X, proj3Y;
    projectOntoSegment(px, py, topX, topY, leftX, leftY, proj1X, proj1Y);
    projectOntoSegment(px, py, leftX, leftY, rightX, rightY, proj2X, proj2Y);
    projectOntoSegment(px, py, rightX, rightY, topX, topY, proj3X, proj3Y);

    // Find closest projection
    double d1 = distanceSq(px, py, proj1X, proj1Y);
    double d2 = distanceSq(px, py, proj2X, proj2Y);
    double d3 = distanceSq(px, py, proj3X, proj3Y);

    if (d1 <= d2 && d1 <= d3) {
        outX = proj1X;
        outY = proj1Y;
    } else if (d2 <= d3) {
        outX = proj2X;
        outY = proj2Y;
    } else {
        outX = proj3X;
        outY = proj3Y;
    }
}

// Optimized: Convert cartesian to barycentric
inline void cartesianToBary(double px, double py, BarycentricCoord& result) {
    // Clamp first
    double clampedX, clampedY;
    clampToTriangle(px, py, clampedX, clampedY);

    // Calculate barycentric coordinates using pre-calculated inverse area
    double perf = signedArea(clampedX, clampedY, leftX, leftY, rightX, rightY) * invTotalArea;
    double vel = signedArea(clampedX, clampedY, rightX, rightY, topX, topY) * invTotalArea;
    double adapt = signedArea(clampedX, clampedY, topX, topY, leftX, leftY) * invTotalArea;

    result.performance = std::max(0.0, std::min(1.0, perf));
    result.velocity = std::max(0.0, std::min(1.0, vel));
    result.adaptability = std::max(0.0, std::min(1.0, adapt));
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init() {
        const int width = 700;
        const int height = 550;
        const int padding = 80;

        topX = width / 2.0;
        topY = padding;
        leftX = padding;
        leftY = height - padding;
        rightX = width - padding;
        rightY = height - padding;

        // Pre-calculate inverse of total area for optimization
        double totalArea = signedArea(topX, topY, leftX, leftY, rightX, rightY);
        invTotalArea = (std::abs(totalArea) < 0.001) ? 1.0 : (1.0 / totalArea);

        dotPositionDirty = true;
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseDown(double mouseX, double mouseY) {
        updateDotCartesian();

        double distSq = distanceSq(mouseX, mouseY, cachedDotX, cachedDotY);

        // If clicking on dot or inside triangle, start dragging
        if (distSq < 225) { // 15^2 = 225
            isDragging = true;
        } else if (isInsideTriangle(mouseX, mouseY)) {
            cartesianToBary(mouseX, mouseY, dotPosition);
            dotPositionDirty = true;
            isDragging = true;
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseMove(double mouseX, double mouseY) {
        if (!isDragging) return;

        cartesianToBary(mouseX, mouseY, dotPosition);
        dotPositionDirty = true;
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseUp() {
        isDragging = false;
    }

    EMSCRIPTEN_KEEPALIVE
    double getDotX() {
        updateDotCartesian();
        return cachedDotX;
    }

    EMSCRIPTEN_KEEPALIVE
    double getDotY() {
        updateDotCartesian();
        return cachedDotY;
    }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleTopX() { return topX; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleTopY() { return topY; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleLeftX() { return leftX; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleLeftY() { return leftY; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleRightX() { return rightX; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleRightY() { return rightY; }

    EMSCRIPTEN_KEEPALIVE
    double getPerformance() { return dotPosition.performance; }

    EMSCRIPTEN_KEEPALIVE
    double getVelocity() { return dotPosition.velocity; }

    EMSCRIPTEN_KEEPALIVE
    double getAdaptability() { return dotPosition.adaptability; }
}

// emcc optimization_triangle.cpp -o optimization_triangle.js -O3 -s WASM=1 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -s ALLOW_MEMORY_GROWTH=1
