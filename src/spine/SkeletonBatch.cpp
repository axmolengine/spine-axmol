/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated January 1, 2020. Replaces all prior versions.
 *
 * Copyright (c) 2013-2020, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software
 * or otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THE SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <spine/spine-axmol.h>

#include <algorithm>
#include <spine/Extension.h>

USING_NS_AX;
#define EVENT_AFTER_DRAW_RESET_POSITION "director_after_draw"
using std::max;
#define INITIAL_SIZE (10000)

#include "renderer/backend/DriverBase.h"
#include "renderer/Shaders.h"
#include "renderer/backend/Types.h"

namespace spine {

	static SkeletonBatch *instance = nullptr;

	SkeletonBatch *SkeletonBatch::getInstance() {
		if (!instance) instance = new SkeletonBatch();
		return instance;
	}

	void SkeletonBatch::destroyInstance() {
		if (instance) {
			delete instance;
			instance = nullptr;
		}
	}

	SkeletonBatch::SkeletonBatch() {

		auto program = backend::Program::getBuiltinProgram(backend::ProgramType::POSITION_TEXTURE_COLOR);
		_programState = new backend::ProgramState(program);// new default program state
		for (unsigned int i = 0; i < INITIAL_SIZE; i++) {
			_commandsPool.push_back(newCommand());
		}
		reset();
		// callback after drawing is finished so we can clear out the batch state
		// for the next frame
		Director::getInstance()->getEventDispatcher()->addCustomEventListener(EVENT_AFTER_DRAW_RESET_POSITION, [this](EventCustom *eventCustom) {
			this->update(0);
		});
	}

	SkeletonBatch::~SkeletonBatch() {
		Director::getInstance()->getEventDispatcher()->removeCustomEventListeners(EVENT_AFTER_DRAW_RESET_POSITION);

		for (unsigned int i = 0; i < _commandsPool.size(); i++) {
			AX_SAFE_RELEASE(_commandsPool[i]->getPipelineDescriptor().programState);
			delete _commandsPool[i];
			_commandsPool[i] = nullptr;
		}

		AX_SAFE_RELEASE(_programState);
	}

	backend::ProgramState* SkeletonBatch::updateCommandPipelinePS(SkeletonCommand* command, backend::ProgramState* programState)
	{
		auto& currentState = command->getPipelineDescriptor().programState;
	#if defined(AX_VERSION)
                if (currentState == nullptr ||
                    currentState->getBatchId() != programState->getBatchId())
                {
	#else
		if(currentState == nullptr || currentState->getProgram() != programState->getProgram()) {
	#endif
			AX_SAFE_RELEASE(currentState);
			currentState = programState->clone();
		
			command->_locMVP     = currentState->getUniformLocation(backend::UNIFORM_NAME_MVP_MATRIX);
	        command->_locTexture = currentState->getUniformLocation(backend::UNIFORM_NAME_TEXTURE);
		}
		return currentState;
	}

	void SkeletonBatch::update(float delta) {
		reset();
	}

	ax::V3F_C4B_T2F *SkeletonBatch::allocateVertices(uint32_t numVertices) {
		if (_vertices.size() - _numVertices < numVertices) {
			ax::V3F_C4B_T2F *oldData = _vertices.data();
			_vertices.resize((_vertices.size() + numVertices) * 2 + 1);
			ax::V3F_C4B_T2F *newData = _vertices.data();
			for (uint32_t i = 0; i < this->_nextFreeCommand; i++) {
				SkeletonCommand *command = _commandsPool[i];
				SkeletonCommand::Triangles &triangles = (SkeletonCommand::Triangles &) command->getTriangles();
				triangles.verts = newData + (triangles.verts - oldData);
			}
		}

		ax::V3F_C4B_T2F *vertices = _vertices.data() + _numVertices;
		_numVertices += numVertices;
		return vertices;
	}

	void SkeletonBatch::deallocateVertices(uint32_t numVertices) {
		_numVertices -= numVertices;
	}


	unsigned short* SkeletonBatch::allocateIndices(uint32_t numIndices)
    {
        if (_indices.size() - _numIndices < numIndices)
        {
            auto oldSize            = _indices.size();
            unsigned short* oldData = _indices.data();
            _indices.resize((_indices.size() + numIndices) * 2 + 1);
            unsigned short* newData = _indices.data();
            for (uint32_t i = 0; i < this->_nextFreeCommand; i++)
            {
                TrianglesCommand* command = _commandsPool[i];
                ax::TrianglesCommand::Triangles& triangles =
                    (ax::TrianglesCommand::Triangles&)command->getTriangles();
                if (triangles.indices >= oldData && triangles.indices < oldData + oldSize)
                {
                    triangles.indices = newData + (triangles.indices - oldData);
                }
            }
        }

        unsigned short* indices = _indices.data() + _numIndices;
        _numIndices += numIndices;
        return indices;
    }

	void SkeletonBatch::deallocateIndices(uint32_t numIndices) {
        _numIndices -= numIndices;
	}


	ax::TrianglesCommand *SkeletonBatch::addCommand(ax::Renderer *renderer, float globalOrder, ax::Texture2D *texture, backend::ProgramState *programState, ax::BlendFunc blendType, const ax::TrianglesCommand::Triangles &triangles, const ax::Mat4 &mv, uint32_t flags) {
		SkeletonCommand *command = nextFreeCommand();
		const ax::Mat4 &projectionMat = Director::getInstance()->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);

		if (programState == nullptr)
			programState = _programState;

		AXASSERT(programState, "programState should not be null");

		auto pipelinePS = updateCommandPipelinePS(command, programState);

		pipelinePS->setUniform(command->_locMVP, projectionMat.m, sizeof(projectionMat.m));
		pipelinePS->setTexture(command->_locTexture, 0, texture->getBackendTexture());

		command->init(globalOrder, texture, blendType, triangles, mv, flags);
		renderer->addCommand(command);
		return command;
	}

	void SkeletonBatch::reset() {
		_nextFreeCommand = 0;
		_numVertices = 0;
        _numIndices  = 0;
	}

	SkeletonCommand *SkeletonBatch::nextFreeCommand() {
		if (_commandsPool.size() <= _nextFreeCommand) {
			unsigned int newSize = _commandsPool.size() * 2 + 1;
			for (int i = _commandsPool.size(); i < newSize; i++) {
				_commandsPool.push_back(newCommand());
			}
		}
		auto *command = _commandsPool[_nextFreeCommand++];
		return command;
	}

	SkeletonCommand *SkeletonBatch::newCommand() {
		auto *command = new SkeletonCommand();
		return command;
	}
}// namespace spine
