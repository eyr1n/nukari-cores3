#include "NukariFace.h"

#include <algorithm>
#include <cmath>

#include <DrawingUtils.hpp>

namespace {

const uint16_t COLOR_OUTLINE = M5GFX::color24to16(0x2d1b4f);
const uint16_t COLOR_SKIN = M5GFX::color24to16(0xf3ecd2);
const uint16_t COLOR_CHEEK = M5GFX::color24to16(0xf0ccda);
const uint16_t COLOR_IRIS = M5GFX::color24to16(0xc06ef4);
const uint16_t COLOR_HAIR = M5GFX::color24to16(0xe7d9fa);
const uint16_t COLOR_HIGHLIGHT = 0xffff;
const uint16_t COLOR_MOUTH_FILL = M5GFX::color24to16(0xe8968c);
const uint16_t COLOR_MOUTH_LINE = M5GFX::color24to16(0xa8505a);

constexpr float LID_TILT_RAD = 0.15f;
constexpr float DOUBT_MAX_OPEN = 0.8f;
constexpr float TALKING_MIN_OPEN = 0.1f;

constexpr int EYE_TOP = 36;
constexpr int EYE_ELLIPSE_DY = -3;
constexpr int EYE_THICKNESS = 5;
constexpr int LID_TRAVEL = 84;
constexpr int LID_OUTER = 53;
constexpr int LID_INNER = 48;
constexpr int LID_HALF = 3;
constexpr int HIGHLIGHT_X = 32;
constexpr int HIGHLIGHT_Y = 19;
constexpr int HIGHLIGHT_RX = 15;
constexpr int HIGHLIGHT_RY = 7;
constexpr int CREASE_X = 34;
constexpr int CREASE_GAP = 14;
constexpr int CREASE_W = 20;
constexpr int CREASE_H = 5;

constexpr int MOUTH_ANCHOR_X = 160;
constexpr int MOUTH_ANCHOR_Y = 229;
constexpr int MIRROR_SUM = 319;
constexpr int TUFT_OVERHANG = 3; // 呼吸の振幅だけ画面上にはみ出させる

struct Point {
  float x;
  float y;
};

struct Curve {
  Point top;
  Point ctrl;
  Point tip;
};

struct Tuft {
  Curve left;
  Curve right;
  bool symmetric;
};

const Tuft HAIR_TUFTS[] = {
    {{{100, 0}, {117, 54}, {159.5f, 90}}, {}, true},
    {{{40, 0}, {46, 41}, {84, 58}}, {{92, 0}, {92, 30}, {84, 58}}, false},
    {{{0, 0}, {0, 24}, {0, 48}}, {{36, 0}, {28, 36}, {0, 48}}, false},
};

int mirrorX(int x) { return MIRROR_SUM - x; }

float curveX(const Curve &c, float y) {
  float a = c.tip.y - 2 * c.ctrl.y;
  float b = 2 * c.ctrl.y;
  float t = a == 0 ? y / b : (-b + std::sqrt(b * b + 4 * a * y)) / (2 * a);
  float u = 1 - t;
  return u * u * c.top.x + 2 * u * t * c.ctrl.x + t * t * c.tip.x;
}

void fillRow(M5Canvas *canvas, int offset_x, int offset_y, int y, int left, int right, bool mirrored) {
  if (right < left) {
    return;
  }
  canvas->drawFastHLine(offset_x + left, offset_y + y, right - left + 1, COLOR_HAIR);
  if (mirrored) {
    canvas->drawFastHLine(offset_x + mirrorX(right), offset_y + y, right - left + 1, COLOR_HAIR);
  }
}

void fillTuft(M5Canvas *canvas, int offset_x, int offset_y, const Tuft &tuft) {
  int tip_y = static_cast<int>(tuft.left.tip.y);
  for (int y = -TUFT_OVERHANG; y <= tip_y; y++) {
    float curve_y = static_cast<float>(std::max(y, 0));
    int left = static_cast<int>(std::ceil(curveX(tuft.left, curve_y)));
    int right = tuft.symmetric ? mirrorX(left) : static_cast<int>(std::floor(curveX(tuft.right, curve_y)));
    fillRow(canvas, offset_x, offset_y, y, left, right, !tuft.symmetric);
  }
}

} // namespace

void NukariEye::overwriteOpenRatio() {
  switch (expression_) {
  case m5avatar::Expression::Doubt:
    open_ratio_ = std::min(open_ratio_, DOUBT_MAX_OPEN);
    break;
  case m5avatar::Expression::Sleepy:
    open_ratio_ = 0.0f;
    break;
  default:
    break;
  }
}

void NukariEye::drawSmile(M5Canvas *canvas) {
  int cy = center_y_ + 26;
  canvas->setClipRect(center_x_ - 50, cy - 50, 101, 51);
  canvas->fillEllipse(center_x_, cy, 50, 50, primary_color_);
  canvas->fillEllipse(center_x_, cy + 5, 46, 46, background_color_);
  canvas->clearClipRect();
}

void NukariEye::drawClosedEye(M5Canvas *canvas) {
  int cy = center_y_ + 10;
  canvas->setClipRect(center_x_ - 48, cy, 97, 21);
  canvas->fillEllipse(center_x_, cy, 48, 20, primary_color_);
  canvas->fillEllipse(center_x_, cy, 48, 11, background_color_);
  canvas->clearClipRect();
}

void NukariEye::drawEyeball(M5Canvas *canvas, int inset, uint16_t color) {
  int top = center_y_ - EYE_TOP;
  int ellipse_y = center_y_ + EYE_ELLIPSE_DY;
  canvas->setClipRect(center_x_ - this->width_ / 2, top, this->width_ + 1, ellipse_y + this->height_ / 2 - top + 1);
  canvas->fillEllipse(center_x_, ellipse_y, this->width_ / 2 - inset, this->height_ / 2 - inset, color);
  canvas->clearClipRect();
}

void NukariEye::drawHighlight(M5Canvas *canvas) {
  int outer = this->is_left_ ? 1 : -1;
  canvas->fillEllipse(center_x_ + outer * HIGHLIGHT_X, center_y_ + HIGHLIGHT_Y, HIGHLIGHT_RX, HIGHLIGHT_RY,
                      COLOR_HIGHLIGHT);
}

void NukariEye::drawEyeLid(M5Canvas *canvas) {
  int top = center_y_ - EYE_TOP;
  int inner = this->is_left_ ? -1 : 1;
  float tilt = 0.0f;
  if (expression_ == m5avatar::Expression::Angry) {
    tilt = inner * LID_TILT_RAD;
  } else if (expression_ == m5avatar::Expression::Sad) {
    tilt = -inner * LID_TILT_RAD;
  }
  int lid_y = static_cast<int>(top + (1.0f - open_ratio_) * LID_TRAVEL);
  int left_x = center_x_ - this->width_ / 2 - 1;
  int right_x = center_x_ + this->width_ / 2 + 1;
  int pivot_x = tilt > 0 ? left_x : tilt < 0 ? right_x : center_x_;

  if (lid_y > top || tilt != 0) {
    constexpr int margin = 20;
    canvas->setClipRect(left_x, top - 1, right_x - left_x + 1, LID_TRAVEL + margin);
    m5avatar::fillRectRotatedAround(canvas, left_x - margin, top - 1 - margin, right_x + margin, lid_y, tilt, pivot_x,
                                    lid_y, background_color_);
    canvas->clearClipRect();
  }

  int lid_left = center_x_ - (inner > 0 ? LID_OUTER : LID_INNER);
  int lid_right = center_x_ + (inner > 0 ? LID_INNER : LID_OUTER);
  m5avatar::fillRectRotatedAround(canvas, lid_left, lid_y - LID_HALF, lid_right, lid_y + LID_HALF, tilt, pivot_x, lid_y,
                                  primary_color_);

  int crease_x = center_x_ + inner * CREASE_X;
  float crease_y = lid_y + (crease_x - pivot_x) * std::sin(tilt) - CREASE_GAP;
  m5avatar::fillRectRotatedAround(canvas, crease_x - CREASE_W / 2, crease_y - CREASE_H / 2, crease_x + CREASE_W / 2,
                                  crease_y + CREASE_H / 2, tilt, crease_x, crease_y, primary_color_);
}

void NukariEye::draw(M5Canvas *canvas, m5avatar::BoundingRect rect, m5avatar::DrawContext *ctx) {
  this->update(canvas, rect, ctx);
  if (expression_ == m5avatar::Expression::Happy) {
    this->drawSmile(canvas);
    return;
  }
  this->overwriteOpenRatio();
  if (open_ratio_ <= 0) {
    this->drawClosedEye(canvas);
    return;
  }

  this->drawEyeball(canvas, 0, primary_color_);
  this->drawEyeball(canvas, EYE_THICKNESS, COLOR_IRIS);
  this->drawHighlight(canvas);
  this->drawEyeLid(canvas);
}

void NukariMouth::drawHair(M5Canvas *canvas) {
  int offset_x = center_x_ - MOUTH_ANCHOR_X;
  int offset_y = center_y_ - MOUTH_ANCHOR_Y;
  for (const Tuft &tuft : HAIR_TUFTS) {
    fillTuft(canvas, offset_x, offset_y, tuft);
  }
}

void NukariMouth::drawCheeks(M5Canvas *canvas) {
  canvas->fillEllipse(center_x_ - 142, center_y_ - 37, 32, 17, secondary_color_);
  canvas->fillEllipse(center_x_ + 141, center_y_ - 37, 32, 17, secondary_color_);
}

void NukariMouth::drawOpenMouth(M5Canvas *canvas, int rx, int ry) {
  canvas->fillEllipse(center_x_, center_y_, rx, ry, COLOR_MOUTH_LINE);
  canvas->fillEllipse(center_x_, center_y_, rx - 2, ry - 2, COLOR_MOUTH_FILL);
}

void NukariMouth::drawClosedMouth(M5Canvas *canvas, m5avatar::Expression expression) {
  switch (expression) {
  case m5avatar::Expression::Happy:
    canvas->setClipRect(center_x_ - 22, center_y_ - 3, 45, 8);
    canvas->fillEllipse(center_x_, center_y_ - 3, 22, 7, COLOR_MOUTH_LINE);
    canvas->fillEllipse(center_x_, center_y_ - 3, 20, 5, COLOR_MOUTH_FILL);
    canvas->clearClipRect();
    canvas->fillRect(center_x_ - 22, center_y_ - 4, 45, 2, COLOR_MOUTH_LINE);
    break;
  case m5avatar::Expression::Sad:
    canvas->fillArc(center_x_, center_y_ + 12, 16, 11, 210, 330, primary_color_);
    break;
  case m5avatar::Expression::Angry:
    canvas->fillRect(center_x_ - 13, center_y_, 26, 5, primary_color_);
    break;
  case m5avatar::Expression::Doubt:
  case m5avatar::Expression::Sleepy:
    this->drawOpenMouth(canvas, 14, 9);
    break;
  default:
    canvas->fillRect(center_x_ - 18, center_y_ - 2, 36, 5, primary_color_);
    break;
  }
}

void NukariMouth::draw(M5Canvas *canvas, m5avatar::BoundingRect rect, m5avatar::DrawContext *ctx) {
  this->update(canvas, rect, ctx);
  this->drawHair(canvas);
  this->drawCheeks(canvas);
  if (open_ratio_ >= TALKING_MIN_OPEN) {
    float w = min_width_ + (max_width_ - min_width_) * open_ratio_;
    float h = min_height_ + (max_height_ - min_height_) * open_ratio_;
    this->drawOpenMouth(canvas, static_cast<int>(w / 2), static_cast<int>(h / 2));
  } else {
    this->drawClosedMouth(canvas, ctx->getExpression());
  }
}

NukariFace::NukariFace()
    : Face(new NukariMouth(20, 28, 6, 24), new m5avatar::BoundingRect(229, 160), new NukariEye(86, 126, false),
           new m5avatar::BoundingRect(104, 51), new NukariEye(86, 126, true), new m5avatar::BoundingRect(104, 268),
           new m5avatar::EllipseEyebrow(0, 0, false), new m5avatar::BoundingRect(54, 85),
           new m5avatar::EllipseEyebrow(0, 0, true), new m5avatar::BoundingRect(54, 234)) {}

NukariColorPalette::NukariColorPalette() {
  this->set(COLOR_PRIMARY, COLOR_OUTLINE);
  this->set(COLOR_SECONDARY, COLOR_CHEEK);
  this->set(COLOR_BACKGROUND, COLOR_SKIN);
}
