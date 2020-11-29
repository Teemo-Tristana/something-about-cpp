#ifndef __GEO_H__
#define __GEO_H__

#include "server.h"

/* Structures used inside geo.c in order to represent points and array of
 * points on the earth. */
/**
 * 在 geo.c 内部中使用的数据结构，
 * 用于表示在地球上的位置(地球上某一点)
*/
typedef struct geoPoint {
    // 经度
    double longitude;

    // 纬度
    double latitude;

    // (此经纬度的点与另外一个经纬度点之间的)距离
    double dist;

    // (经纬度计算出的)分值
    double score;

    // 对应于有序集合成员
    char *member;
} geoPoint;

// geoArray 用于存储 geoPoint 的数组
typedef struct geoArray {
    // 数组
    struct geoPoint *array; 

    // 数组容量(类似vector的capacity)
    size_t buckets;

    // 数组已有个数(类似vector的size)
    size_t used;
} geoArray;

#endif
