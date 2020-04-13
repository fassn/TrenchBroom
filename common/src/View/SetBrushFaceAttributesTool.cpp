/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SetBrushFaceAttributesTool.h"

#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/BrushFaceHandle.h"
#include "Model/BrushNode.h"
#include "Model/ChangeBrushFaceAttributesRequest.h"
#include "Model/HitAdapter.h"
#include "Model/HitQuery.h"
#include "Model/TexCoordSystem.h"
#include "View/InputState.h"
#include "View/MapDocument.h"

#include <kdl/memory_utils.h>

#include <vector>

#include "Ensure.h"

namespace TrenchBroom {
    namespace View {
        SetBrushFaceAttributesTool::SetBrushFaceAttributesTool(std::weak_ptr<MapDocument> document) :
        ToolControllerBase(),
        Tool(true),
        m_document(document) {}

        Tool* SetBrushFaceAttributesTool::doGetTool() {
            return this;
        }

        const Tool* SetBrushFaceAttributesTool::doGetTool() const {
            return this;
        }

        bool SetBrushFaceAttributesTool::doMouseClick(const InputState& inputState) {
            if (canCopyAttributesFromSelection(inputState)) {
                copyAttributesFromSelection(inputState, false);
                return true;
            } else {
                return false;
            }
        }

        bool SetBrushFaceAttributesTool::doMouseDoubleClick(const InputState& inputState) {
            if (canCopyAttributesFromSelection(inputState)) {
                // A double click is always preceeded by a single click, so we already done some work which is now
                // superseded by what is done next. To avoid inconsistencies with undo, we undo the work done by the
                // single click now:
                auto document = kdl::mem_lock(m_document);
                document->undoCommand();

                copyAttributesFromSelection(inputState, true);
                return true;
            } else {
                return false;
            }
        }

        static Model::WrapStyle wrapStyle(const InputState& inputState) {
            return inputState.modifierKeysDown(ModifierKeys::MKShift) ? Model::WrapStyle::Rotation : Model::WrapStyle::Projection;
        }

        void SetBrushFaceAttributesTool::copyAttributesFromSelection(const InputState& inputState, const bool applyToBrush) {
            assert(canCopyAttributesFromSelection(inputState));

            auto document = kdl::mem_lock(m_document);

            const auto selectedFaces = document->selectedBrushFaces();
            assert(!selectedFaces.empty());

            const Model::Hit& hit = inputState.pickResult().query().pickable().type(Model::BrushNode::BrushHitType).occluded().first();

            Model::BrushNode* sourceBrush = selectedFaces.front().node();
            Model::BrushFace* sourceFace = selectedFaces.front().face();
            Model::BrushNode* targetBrush = Model::hitToBrush(hit);
            Model::BrushFace* targetFace = Model::hitToFace(hit);
            const auto targetList = applyToBrush ? Model::toHandles(targetBrush) : std::vector<Model::BrushFaceHandle>{Model::BrushFaceHandle(targetBrush, targetFace)};

            const Model::WrapStyle style = wrapStyle(inputState);

            const Transaction transaction(document);
            document->deselectAll();
            document->select(targetList);
            if (copyAllAttributes(inputState)) {
                auto snapshot = sourceFace->takeTexCoordSystemSnapshot();
                document->setFaceAttributes(sourceFace->attributes());
                if (snapshot != nullptr) {
                    document->copyTexCoordSystemFromFace(*snapshot, sourceFace->attributes().takeSnapshot(), sourceFace->boundary(), style);
                }
            } else {
                Model::ChangeBrushFaceAttributesRequest request;
                request.setTextureName(sourceFace->attributes().textureName());
                if (document->setFaceAttributes(request)) {
                    document->setCurrentTextureName(sourceFace->attributes().textureName());
                }
            }
            document->deselectAll();
            document->select({ sourceBrush, sourceFace });
        }

        bool SetBrushFaceAttributesTool::canCopyAttributesFromSelection(const InputState& inputState) const {
            if (!applies(inputState)) {
                return false;
            }

            auto document = kdl::mem_lock(m_document);

            const auto selectedFaces = document->selectedBrushFaces();
            if (selectedFaces.size() != 1)
                return false;

            const Model::Hit& hit = inputState.pickResult().query().pickable().type(Model::BrushNode::BrushHitType).occluded().first();
            if (!hit.isMatch())
                return false;

            return true;
        }

        bool SetBrushFaceAttributesTool::applies(const InputState& inputState) const {
            return inputState.checkModifierKeys(MK_DontCare, MK_Yes, MK_DontCare);
        }

        bool SetBrushFaceAttributesTool::copyAllAttributes(const InputState& inputState) const {
            return !inputState.modifierKeysDown(ModifierKeys::MKCtrlCmd);
        }

        bool SetBrushFaceAttributesTool::doCancel() {
            return false;
        }

        bool SetBrushFaceAttributesTool::doStartMouseDrag(const InputState& inputState) {
            if (!applies(inputState)) {
                return false;
            }

            auto document = kdl::mem_lock(m_document);

            // Need to have a selected face to start painting alignment
            const std::vector<Model::BrushFaceHandle>& selectedFaces = document->selectedBrushFaces();
            if (selectedFaces.size() != 1) {
                return false;
            }

            resetDragState();
            m_dragInitialSelectedFaceHandle = selectedFaces[0];

            document->startTransaction("Drag Apply Face Attributes");

            return true;
        }

        bool SetBrushFaceAttributesTool::doMouseDrag(const InputState& inputState) {            
            const Model::Hit& hit = inputState.pickResult().query().pickable().type(Model::BrushNode::BrushHitType).occluded().first();
            const auto faceHandle = Model::hitToFaceHandle(hit);
            if (!faceHandle) {
                // Dragging over void
                return true;
            }

            if (faceHandle == m_dragTargetFaceHandle) {
                // Dragging on the same face as last frame
                return true;
            }

            if (!m_dragTargetFaceHandle && !m_dragTargetFaceHandle) {
                // Start drag
                m_dragSourceFaceHandle = m_dragInitialSelectedFaceHandle;
                m_dragTargetFaceHandle = faceHandle;
            } else {
                // Continuing drag onto new face
                m_dragSourceFaceHandle = m_dragTargetFaceHandle;
                m_dragTargetFaceHandle = faceHandle;
            }

            transferFaceAttributes(inputState, *m_dragSourceFaceHandle, *m_dragTargetFaceHandle);

            return true;
        }

        void SetBrushFaceAttributesTool::doEndMouseDrag(const InputState&) {
            auto document = kdl::mem_lock(m_document);
            document->commitTransaction();

            resetDragState();
        }

        void SetBrushFaceAttributesTool::doCancelMouseDrag() {
            auto document = kdl::mem_lock(m_document);
            document->cancelTransaction();

            resetDragState();
        }

        void SetBrushFaceAttributesTool::resetDragState() {
            m_dragInitialSelectedFaceHandle = std::nullopt;
            m_dragTargetFaceHandle = std::nullopt;
            m_dragSourceFaceHandle = std::nullopt;
        }

        void SetBrushFaceAttributesTool::transferFaceAttributes(const InputState& inputState, const Model::BrushFaceHandle& sourceFaceHandle, const Model::BrushFaceHandle& targetFaceHandle) {
            ensure(m_dragInitialSelectedFaceHandle, "no initially selected face");

            auto document = kdl::mem_lock(m_document);

            const Model::WrapStyle style = wrapStyle(inputState);

            const Transaction transaction(document);
            document->deselectAll();
            document->select(targetFaceHandle);

            auto snapshot = sourceFaceHandle.face()->takeTexCoordSystemSnapshot();
            document->setFaceAttributes(sourceFaceHandle.face()->attributes());
            if (snapshot != nullptr) {
                document->copyTexCoordSystemFromFace(*snapshot, sourceFaceHandle.face()->attributes().takeSnapshot(), sourceFaceHandle.face()->boundary(), style);
            }

            document->deselectAll();
            document->select(*m_dragInitialSelectedFaceHandle);
        }
    }
}
