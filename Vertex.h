//
//  Vertex.h
//  TrenchBroom
//
//  Created by Kristian Duske on 25.01.11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef enum {
    VM_DROP,
    VM_KEEP,
    VM_NEW,
    VM_UNKNOWN
} EVertexMark;

@class Vector3f;

@interface Vertex : NSObject {
    @private
    Vector3f* vector;
    EVertexMark mark;
}

- (id)initWithVector:(Vector3f *)theVector;

- (Vector3f *)vector;
- (EVertexMark)mark;
- (void)setMark:(EVertexMark)theMark;

@end
