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
#include <spine/Extension.h>
#include <spine/AttachmentVertices.h>

USING_NS_AX;
using namespace spine;

static void deleteAttachmentVertices (void* vertices) {
	delete (AttachmentVertices *) vertices;
}

static unsigned short quadTriangles[6] = {0, 1, 2, 2, 3, 0};

static void setAttachmentVertices(RegionAttachment* attachment) {
	AtlasRegion* region = (AtlasRegion*)attachment->getRendererObject();
	AttachmentVertices* attachmentVertices = new AttachmentVertices((Texture2D*)region->page->getRendererObject(), 4, quadTriangles, 6);
	V3F_C4B_T2F* vertices = attachmentVertices->_triangles->verts;
	for (int i = 0, ii = 0; i < 4; ++i, ii += 2) {
		vertices[i].texCoords.u = attachment->getUVs()[ii];
		vertices[i].texCoords.v = attachment->getUVs()[ii + 1];
	}
	attachment->setRendererObject(attachmentVertices, deleteAttachmentVertices);
}

static void setAttachmentVertices(MeshAttachment* attachment) {
	AtlasRegion* region = (AtlasRegion*)attachment->getRendererObject();
	AttachmentVertices* attachmentVertices = new AttachmentVertices((Texture2D*)region->page->getRendererObject(),
																	attachment->getWorldVerticesLength() >> 1, attachment->getTriangles().buffer(), attachment->getTriangles().size());
	V3F_C4B_T2F* vertices = attachmentVertices->_triangles->verts;
	for (int i = 0, ii = 0, nn = attachment->getWorldVerticesLength(); ii < nn; ++i, ii += 2) {
		vertices[i].texCoords.u = attachment->getUVs()[ii];
		vertices[i].texCoords.v = attachment->getUVs()[ii + 1];
	}
	attachment->setRendererObject(attachmentVertices, deleteAttachmentVertices);
}

AxmolAtlasAttachmentLoader::AxmolAtlasAttachmentLoader(Atlas* atlas): AtlasAttachmentLoader(atlas) {
}

AxmolAtlasAttachmentLoader::~AxmolAtlasAttachmentLoader() { }

void AxmolAtlasAttachmentLoader::configureAttachment(Attachment* attachment) {
	if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
		setAttachmentVertices((RegionAttachment*)attachment);
	} else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
		setAttachmentVertices((MeshAttachment*)attachment);
	}
}

backend::SamplerAddressMode wrap (TextureWrap wrap) {
	return wrap ==  TextureWrap_ClampToEdge ? backend::SamplerAddressMode::CLAMP_TO_EDGE : backend::SamplerAddressMode::REPEAT;
}

backend::SamplerFilter filter (TextureFilter filter) {
	switch (filter) {
	case TextureFilter_Unknown:
		break;
	case TextureFilter_Nearest:
		return backend::SamplerFilter::NEAREST;
	case TextureFilter_Linear:
		return backend::SamplerFilter::LINEAR;
	case TextureFilter_MipMap:
		return backend::SamplerFilter::LINEAR;
	case TextureFilter_MipMapNearestNearest:
		return backend::SamplerFilter::NEAREST;
	case TextureFilter_MipMapLinearNearest:
        return backend::SamplerFilter::NEAREST;
	case TextureFilter_MipMapNearestLinear:
        return backend::SamplerFilter::LINEAR;
	case TextureFilter_MipMapLinearLinear:
        return backend::SamplerFilter::LINEAR;
	}
	return backend::SamplerFilter::LINEAR;
}

AxmolTextureLoader::AxmolTextureLoader() : TextureLoader() { }
AxmolTextureLoader::~AxmolTextureLoader() { }

void AxmolTextureLoader::load(AtlasPage& page, const spine::String& path) {
	Texture2D* texture = Director::getInstance()->getTextureCache()->addImage(path.buffer());
	AXASSERT(texture != nullptr, "Invalid image");
	if (texture) {
		texture->retain();
		Texture2D::TexParams textureParams(filter(page.minFilter), filter(page.magFilter), wrap(page.uWrap), wrap(page.vWrap));
		texture->setTexParameters(textureParams);

		page.setRendererObject(texture);
		page.width = texture->getPixelsWide();
		page.height = texture->getPixelsHigh();
	}
}

void AxmolTextureLoader::unload(void* texture) {
	if (texture) {
		((Texture2D*)texture)->release();
	}
}


AxmolExtension::AxmolExtension() : DefaultSpineExtension() { }

AxmolExtension::~AxmolExtension() { }

char *AxmolExtension::_readFile(const spine::String &path, int *length) {
    Data data = FileUtils::getInstance()->getDataFromFile(path.buffer());
	if (data.isNull()) return nullptr;

	// avoid buffer overflow (int is shorter than ssize_t in certain platforms)
	ssize_t tmpLen;
	char *ret = (char*)data.takeBuffer(&tmpLen);
	*length = static_cast<int>(tmpLen);
	return ret;
}

SpineExtension *spine::getDefaultExtension () {
	return new AxmolExtension();
}
