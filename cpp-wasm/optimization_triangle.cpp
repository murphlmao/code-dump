#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cmath>
#include <algorithm>

// Structs for coordinate representation
struct Point2D {
    double x;
    double y;

    Point2D() : x(0), y(0) {}
    Point2D(double x, double y) : x(x), y(y) {}
};

struct BarycentricCoord {
    double performance;
    double velocity;
    double adaptability;

    BarycentricCoord() : performance(0.33), velocity(0.33), adaptability(0.34) {}
    BarycentricCoord(double p, double v, double a)
        : performance(p), velocity(v), adaptability(a) {}
};

// Triangle class
class Triangle {
private:
    Point2D top, left, right;

public:
    Triangle(int canvasWidth, int canvasHeight) {
        const int padding = 80;
        top = Point2D(canvasWidth / 2.0, padding);
        left = Point2D(padding, canvasHeight - padding);
        right = Point2D(canvasWidth - padding, canvasHeight - padding);
    }

    Point2D baryToCartesian(const BarycentricCoord& bc) const {
        return Point2D(
            bc.performance * top.x + bc.velocity * left.x + bc.adaptability * right.x,
            bc.performance * top.y + bc.velocity * left.y + bc.adaptability * right.y
        );
    }

    // Calculate signed area of triangle
    double signedArea(const Point2D& p1, const Point2D& p2, const Point2D& p3) const {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    }

    bool isInside(const Point2D& point) const {
        double d1 = signedArea(point, top, left);
        double d2 = signedArea(point, left, right);
        double d3 = signedArea(point, right, top);

        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

        return !(hasNeg && hasPos);
    }

    // Project a point onto a line segment
    Point2D projectPointOntoSegment(const Point2D& p, const Point2D& a, const Point2D& b) const {
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double lengthSq = dx * dx + dy * dy;

        if (lengthSq < 0.001) return a;

        // Calculate projection parameter t
        double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSq;
        t = std::max(0.0, std::min(1.0, t)); // Clamp to segment

        return Point2D(a.x + t * dx, a.y + t * dy);
    }

    // Find closest point on triangle boundary
    Point2D clampToTriangle(const Point2D& point) const {
        if (isInside(point)) return point;

        // Project onto each edge and find closest
        Point2D proj1 = projectPointOntoSegment(point, top, left);
        Point2D proj2 = projectPointOntoSegment(point, left, right);
        Point2D proj3 = projectPointOntoSegment(point, right, top);

        auto distSq = [](const Point2D& a, const Point2D& b) {
            double dx = b.x - a.x;
            double dy = b.y - a.y;
            return dx * dx + dy * dy;
        };

        double d1 = distSq(point, proj1);
        double d2 = distSq(point, proj2);
        double d3 = distSq(point, proj3);

        if (d1 <= d2 && d1 <= d3) return proj1;
        if (d2 <= d3) return proj2;
        return proj3;
    }

    BarycentricCoord cartesianToBary(const Point2D& point) const {
        // Clamp point to triangle first
        Point2D clamped = clampToTriangle(point);

        double totalArea = signedArea(top, left, right);

        if (std::abs(totalArea) < 0.001) totalArea = 1.0;

        double perf = signedArea(clamped, left, right) / totalArea;   // performance (top)
        double vel = signedArea(clamped, right, top) / totalArea;     // velocity (left)
        double adapt = signedArea(clamped, top, left) / totalArea;    // adaptability (right)

        return BarycentricCoord(
            std::max(0.0, std::min(1.0, perf)),
            std::max(0.0, std::min(1.0, vel)),
            std::max(0.0, std::min(1.0, adapt))
        );
    }

    Point2D getTop() const { return top; }
    Point2D getLeft() const { return left; }
    Point2D getRight() const { return right; }
};

// Global state
static Triangle* triangle = nullptr;
static BarycentricCoord dotPosition(0.33, 0.33, 0.34);
static bool isDragging = false;
static const int CANVAS_WIDTH = 700;
static const int CANVAS_HEIGHT = 550;

// Exported functions for JavaScript
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init() {
        triangle = new Triangle(CANVAS_WIDTH, CANVAS_HEIGHT);
    }

    EMSCRIPTEN_KEEPALIVE
    void cleanup() {
        delete triangle;
        triangle = nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseDown(double mouseX, double mouseY) {
        if (!triangle) return;

        Point2D mousePos(mouseX, mouseY);
        Point2D dotPos = triangle->baryToCartesian(dotPosition);

        double dx = mousePos.x - dotPos.x;
        double dy = mousePos.y - dotPos.y;
        double distance = std::sqrt(dx * dx + dy * dy);

        // If clicking on the dot, start dragging
        if (distance < 15) {
            isDragging = true;
        }
        // If clicking inside the triangle, move dot there and start dragging
        else if (triangle->isInside(mousePos)) {
            dotPosition = triangle->cartesianToBary(mousePos);
            isDragging = true;
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseMove(double mouseX, double mouseY) {
        if (!triangle || !isDragging) return;

        Point2D mousePos(mouseX, mouseY);
        dotPosition = triangle->cartesianToBary(mousePos);
    }

    EMSCRIPTEN_KEEPALIVE
    void handleMouseUp() {
        isDragging = false;
    }

    EMSCRIPTEN_KEEPALIVE
    double getDotX() {
        if (!triangle) return 0;
        Point2D pos = triangle->baryToCartesian(dotPosition);
        return pos.x;
    }

    EMSCRIPTEN_KEEPALIVE
    double getDotY() {
        if (!triangle) return 0;
        Point2D pos = triangle->baryToCartesian(dotPosition);
        return pos.y;
    }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleTopX() { return triangle ? triangle->getTop().x : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleTopY() { return triangle ? triangle->getTop().y : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleLeftX() { return triangle ? triangle->getLeft().x : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleLeftY() { return triangle ? triangle->getLeft().y : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleRightX() { return triangle ? triangle->getRight().x : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getTriangleRightY() { return triangle ? triangle->getRight().y : 0; }

    EMSCRIPTEN_KEEPALIVE
    double getPerformance() { return dotPosition.performance; }

    EMSCRIPTEN_KEEPALIVE
    double getVelocity() { return dotPosition.velocity; }

    EMSCRIPTEN_KEEPALIVE
    double getAdaptability() { return dotPosition.adaptability; }
}

/*
 * TO COMPILE:
 * emcc optimization_triangle.cpp -o optimization_triangle.js -s WASM=1 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -s ALLOW_MEMORY_GROWTH=1
 */