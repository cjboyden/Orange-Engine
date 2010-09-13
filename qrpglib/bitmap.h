#ifndef BITMAP_GL_H
#define BITMAP_GL_H 1
#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <QtCore>
#include <QString>
#include "globals.h"
#include "resource.h"

class Resource;

class Bitmap {
public:
  Bitmap(QString image, int spr_w, int spr_h, 
	 int origin_x, int origin_y, QString bmpname = "");
  ~Bitmap();
  bool SetName(QString n);
  void Draw(int tile, float x, float y, float opacity = 1.0, float scale = 1.0);
  void DrawBoundingBox(int tile, float x, float y);
  int Tiles();
  void GetSize(int &w, int &h) { w = width; h = height; }
  void Stub();
  void Unstub();
  QString GetName() { return name; };
  void Save(QString filename);
private:
  int pow2(int x);
  GLuint load_texture(QString name);
  GLuint gl_texture;

  int width, height, x_origin, y_origin;
  int tex_w, tex_h;

  bool stub;
  QString name;
  QString image;
  QString filePath;

  struct Tile {
    float x1, y1, x2, y2;
  };
  QList < Tile * > tiles;

  Resource * thisBitmap;
};

class BitmapReader : public QXmlStreamReader 
{
public:
  Bitmap * read(QIODevice * device);
  Bitmap * read(QString filename);

private:
  void readBitmap();
  void readUnknownElement();
  
  Bitmap * bitmap;
  QFileInfo fileinfo;
};



#endif
