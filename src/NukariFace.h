#pragma once

#include <Avatar.h>
#include <Eyebrows.hpp>
#include <Eyes.hpp>
#include <Face.h>
#include <Mouths.hpp>

class NukariEye : public m5avatar::BaseEye {
public:
  using m5avatar::BaseEye::BaseEye;

  void overwriteOpenRatio();
  void drawSmile(M5Canvas *canvas);
  void drawClosedEye(M5Canvas *canvas);
  void drawEyeball(M5Canvas *canvas, int inset, uint16_t color);
  void drawHighlight(M5Canvas *canvas);
  void drawEyeLid(M5Canvas *canvas);
  void draw(M5Canvas *canvas, m5avatar::BoundingRect rect, m5avatar::DrawContext *ctx);
};

class NukariMouth : public m5avatar::BaseMouth {
public:
  using m5avatar::BaseMouth::BaseMouth;

  void drawHair(M5Canvas *canvas);
  void drawCheeks(M5Canvas *canvas);
  void drawOpenMouth(M5Canvas *canvas, int rx, int ry);
  void drawClosedMouth(M5Canvas *canvas, m5avatar::Expression expression);
  void draw(M5Canvas *canvas, m5avatar::BoundingRect rect, m5avatar::DrawContext *ctx);
};

class NukariFace : public m5avatar::Face {
public:
  NukariFace();
};

class NukariColorPalette : public m5avatar::ColorPalette {
public:
  NukariColorPalette();
};
