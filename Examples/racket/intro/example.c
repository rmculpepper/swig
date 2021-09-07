#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

// Variable definitions

int counter;
const double pi = 3.141592;

int get_counter() { return counter; }

int counter_gte(int v) { return counter >= v; }

void get_set_counter(int *v) {
  int old = counter;
  counter = *v;
  *v = old;
}

// succeeds and returns 0 if *v > 0
// else fails and returns 1
int get_set_counter2(int *v) {
  if (*v > 0) {
    int old = counter;
    counter = *v;
    *v = old;
    return 0;
  } else {
    return 1;
  }
}

// returns sum of alpha chars, or -1 and sets errno if nonalpha char found
int add_alpha_chars(char *s, int len) {
  char *end = s + len;
  int result = 0;

  for (; s < end; s++) {
    if (isalpha(*s)) {
      result += *s;
    } else {
      errno = EINVAL;
      return -1;
    }
  }
  return result;
}



struct point_st {
  int x, y;
};

typedef struct point_st Point;

void reflect_point(struct point_st *p) {
  p->x = -(p->x);
  p->y = -(p->y);
}

Point *flip_point(Point *p) {
  Point *fp = malloc(sizeof(Point));
  fp->x = p->x;
  fp->y = -(p->y);
  return fp;
}

int point_counter = 0;

Point *new_point() {
  point_counter++;
  return malloc(sizeof(Point));
}

void delete_point(Point *p) {
  free(p);
  point_counter--;
}

void mul_intp(int *p, int factor) {
  *p = factor * (*p);
}


enum direction_t { north, east, south, west, up = 100 };

enum direction_t next_direction_cw(enum direction_t d) {
  switch (d) {
  case north: return east;
  case east: return south;
  case south: return west;
  case west: return north;
  case up: return up;
  }
}


enum thing_tag_t { point, direction };

union thing_inner_t {
  Point p;
  enum direction_t d;
};

struct thing_t {
  enum thing_tag_t t;
  union thing_inner_t u;
};

void convert_thing(struct thing_t *t) {
  switch (t->t) {
  case point: {
    if (t->u.p.x == 0 && t->u.p.y == 0) {
      t->u.d = up;
    } else if (abs(t->u.p.x) < abs(t->u.p.y)) {
      if (t->u.p.y > 0) {
        t->u.d = north;
      } else {
        t->u.d = south;
      }
    } else {
      if (t->u.p.x > 0) {
        t->u.d = east;
      } else {
        t->u.d = west;
      }
    }
    t->t = direction;
    break;
  }
  case direction: {
    switch (t->u.d) {
    case north: t->u.p.x = 0;  t->u.p.y = 1;  break;
    case east:  t->u.p.x = 1;  t->u.p.y = 0;  break;
    case south: t->u.p.x = 0;  t->u.p.y = -1; break;
    case west:  t->u.p.x = -1; t->u.p.y = 0;  break;
    case up:    t->u.p.x = 0;  t->u.p.y = 0;  break;
    }
    t->t = point;
    break;
  }
  }
}
