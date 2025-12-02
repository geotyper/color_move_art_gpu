#pragma once

#include <QColor>
#include <QSize>
#include <QVector4D>
#include <QtGui/rhi/qrhi.h>
#include <QtGui/rhi/qrhi_platform.h>
#include <QtGui/private/qrhigles2_p.h>
#include <QSurfaceFormat>
#include <QOffscreenSurface>

#include "mouse_input.h"
#include <memory>
#include <vector>

class QWindow;

class MpmRenderer
{
public:
    explicit MpmRenderer(QWindow *window);
    ~MpmRenderer();

    void resize(const QSize &size);
    void renderFrame(float dtSeconds);
    void onImpulse(const QPointF &pos, const QPointF &vel, float strength);

private:
    struct ParticleGpu {
        QVector4D pos;       // xy: position (pixels), z: unused, w: blue
        QVector4D velColor;  // xy: velocity, z: red, w: green
    };

    bool ensureRhi();
    bool ensureSwapChain();
    void ensurePipelines();
    void createParticleBuffer();
    void updateParticlesCpu(float dtSeconds);
    void ensureCanvas();
    QRhiCommandBuffer *beginFrame(QRhiResourceUpdateBatch *&rub);
    QShader loadShader(const QString &resourcePath) const;

    QWindow *m_window {nullptr};
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QOffscreenSurface> m_fallbackSurface;
    std::unique_ptr<QRhiSwapChain> m_swapChain;
    std::unique_ptr<QRhiRenderPassDescriptor> m_swapChainPassDesc;
    std::unique_ptr<QRhiRenderBuffer> m_depthStencil;
    std::unique_ptr<QRhiShaderResourceBindings> m_splatBindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_presentBindings[2];
    std::unique_ptr<QRhiShaderResourceBindings> m_blurBindings[2];
    std::unique_ptr<QRhiShaderResourceBindings> m_blurComputeBindings[2];
    std::unique_ptr<QRhiGraphicsPipeline> m_splatPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_presentPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_blurPipeline;
    std::unique_ptr<QRhiComputePipeline> m_blurComputePipeline;
    std::unique_ptr<QRhiBuffer> m_particleBuffer;
    std::unique_ptr<QRhiBuffer> m_viewParamsBuf;
    std::unique_ptr<QRhiBuffer> m_instanceBuf;
    std::unique_ptr<QRhiBuffer> m_blurParamsBuf;
    std::unique_ptr<QRhiTexture> m_canvasTex[2];
    std::unique_ptr<QRhiTextureRenderTarget> m_canvasRt[2];
    std::unique_ptr<QRhiRenderPassDescriptor> m_canvasRp[2];
    std::unique_ptr<QRhiSampler> m_sampler;

    std::vector<ParticleGpu> m_particles;
    MouseInput m_input;
    QSize m_viewSize;
    int m_particleCount {256};
    int m_canvasIndex {0};
    bool m_particlesPendingUpload {false};
    bool m_canvasInitialized {false};
    bool m_loggedBlurBackend {false};
};
