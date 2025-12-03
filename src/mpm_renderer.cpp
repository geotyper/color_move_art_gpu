#include "mpm_renderer.h"

#include <QColor>
#include <QFile>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>
#include <QWindow>
#include <algorithm>

namespace {
QSize scaledSize(const QWindow *window)
{
    const qreal dpr = window ? window->devicePixelRatio() : 1.0;
    return (window ? window->size() : QSize(1, 1)) * dpr;
}
} // namespace

MpmRenderer::MpmRenderer(QWindow *window)
    : m_window(window)
{
}

MpmRenderer::~MpmRenderer()
{
    shutdown();
}

void MpmRenderer::resize(const QSize &size)
{
    Q_UNUSED(size);
    m_viewSize = scaledSize(m_window);
    m_swapChain.reset();
    m_swapChainPassDesc.reset();
    m_depthStencil.reset();
    for (int i = 0; i < 2; ++i) {
        m_canvasTex[i].reset();
        m_canvasRt[i].reset();
        m_canvasRp[i].reset();
        m_presentBindings[i].reset();
        m_blurBindings[i].reset();
        m_blurComputeBindings[i].reset();
    }
    m_splatBindings.reset();
    m_presentPipeline.reset();
    m_splatPipeline.reset();
    m_blurPipeline.reset();
    m_blurComputePipeline.reset();
    m_gridClearPipeline.reset();
    m_gridNormalizePipeline.reset();
    m_instanceBuf.reset();
    m_blurParamsBuf.reset();
    m_gridBindings.reset();
    m_gridVel.reset();
    m_gridMass.reset();
    m_canvasInitialized = false;
}

bool MpmRenderer::ensureRhi()
{
    if (m_rhi)
        return true;

    QRhiGles2InitParams params;
    params.format = m_window ? m_window->format() : QSurfaceFormat::defaultFormat();
    m_fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface(params.format));
    params.fallbackSurface = m_fallbackSurface.get();

    m_rhi.reset(QRhi::create(QRhi::OpenGLES2, &params, QRhi::EnableDebugMarkers));

    return m_rhi != nullptr;
}

bool MpmRenderer::ensureSwapChain()
{
    if (!ensureRhi())
        return false;

    if (!m_viewSize.isValid() || m_viewSize.isEmpty())
        m_viewSize = scaledSize(m_window);

    if (m_swapChain && m_swapChain->currentPixelSize() == m_viewSize)
        return true;

    m_depthStencil.reset();
    m_swapChainPassDesc.reset();
    m_swapChain.reset(m_rhi->newSwapChain());
    m_depthStencil.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, m_viewSize, 1));
    m_swapChain->setWindow(m_window);
    m_swapChain->setDepthStencil(m_depthStencil.get());
    m_swapChainPassDesc.reset(m_swapChain->newCompatibleRenderPassDescriptor());
    m_swapChain->setRenderPassDescriptor(m_swapChainPassDesc.get());

    if (!m_swapChain->createOrResize())
        return false;

    if (!m_particleBuffer)
        createParticleBuffer();

    return true;
}

void MpmRenderer::ensureCanvas()
{
    if (!m_rhi)
        return;

    const QSize canvasSize = m_swapChain ? m_swapChain->currentPixelSize() : m_viewSize;
    if (!canvasSize.isValid())
        return;

    for (int i = 0; i < 2; ++i) {
        bool needsCreate = !m_canvasTex[i] || m_canvasTex[i]->pixelSize() != canvasSize;
        if (needsCreate) {
            m_canvasTex[i].reset(m_rhi->newTexture(QRhiTexture::RGBA16F, canvasSize, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore));
            m_canvasTex[i]->create();
            qDebug() << "[canvas]" << i << "created size" << canvasSize;

            QRhiTextureRenderTargetDescription rtDesc;
            rtDesc.setColorAttachments({m_canvasTex[i].get()});
            m_canvasRt[i].reset(m_rhi->newTextureRenderTarget(rtDesc));
            m_canvasRp[i].reset(m_canvasRt[i]->newCompatibleRenderPassDescriptor());
            m_canvasRt[i]->setRenderPassDescriptor(m_canvasRp[i].get());
            m_canvasRt[i]->create();

            m_presentBindings[i].reset();
            m_blurBindings[i].reset();
            m_blurComputeBindings[i].reset();
            m_canvasInitialized = false;
        }

    }
}

void MpmRenderer::ensureGrid()
{
    if (!m_rhi)
        return;

    const QSize base = m_swapChain ? m_swapChain->currentPixelSize() : m_viewSize;
    if (!base.isValid() || base.isEmpty())
        return;

    // Downsample grid for stability/performance; finer grid (half-res) for better cohesion detail.
    m_gridSize = QSize(std::max(64, base.width() / 2), std::max(64, base.height() / 2));

    bool needVel = !m_gridVel || m_gridVel->pixelSize() != m_gridSize;
    bool needMass = !m_gridMass || m_gridMass->pixelSize() != m_gridSize;

    if (needVel) {
        m_gridVel.reset(m_rhi->newTexture(QRhiTexture::RGBA16F, m_gridSize, 1, QRhiTexture::UsedWithLoadStore));
        m_gridVel->create();
    }
    if (needMass) {
        m_gridMass.reset(m_rhi->newTexture(QRhiTexture::R16F, m_gridSize, 1, QRhiTexture::UsedWithLoadStore));
        m_gridMass->create();
    }

    bool needPressure = !m_gridPressure[0] || m_gridPressure[0]->pixelSize() != m_gridSize;

    if (needVel) {
        m_gridVel.reset(m_rhi->newTexture(QRhiTexture::RGBA16F, m_gridSize, 1, QRhiTexture::UsedWithLoadStore));
        m_gridVel->create();
    }
    if (needMass) {
        m_gridMass.reset(m_rhi->newTexture(QRhiTexture::R16F, m_gridSize, 1, QRhiTexture::UsedWithLoadStore));
        m_gridMass->create();
    }
    if (needPressure) {
        for (int i = 0; i < 2; ++i) {
            m_gridPressure[i].reset(m_rhi->newTexture(QRhiTexture::R16F, m_gridSize, 1, QRhiTexture::UsedWithLoadStore));
            m_gridPressure[i]->create();
        }
    }

    if ((needVel || needMass || needPressure) && m_gridVel && m_gridMass) {
        m_gridBindings.reset(m_rhi->newShaderResourceBindings());
        m_gridBindings->setBindings({
            QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_gridVel.get(), 0),
            QRhiShaderResourceBinding::imageLoadStore(1, QRhiShaderResourceBinding::ComputeStage, m_gridMass.get(), 0)
        });
        m_gridBindings->create();

        // Divergence binding: vel, mass, divergence
        m_gridDivergenceBindings.reset(m_rhi->newShaderResourceBindings());
        m_gridDivergenceBindings->setBindings({
            QRhiShaderResourceBinding::imageLoad(0, QRhiShaderResourceBinding::ComputeStage, m_gridVel.get(), 0),
            QRhiShaderResourceBinding::imageLoad(1, QRhiShaderResourceBinding::ComputeStage, m_gridMass.get(), 0),
            QRhiShaderResourceBinding::imageStore(2, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0) // reuse pressure[0] for divergence
        });
        m_gridDivergenceBindings->create();

        // Pressure ping-pong bindings for Jacobi
        m_gridJacobiBindings[0].reset(m_rhi->newShaderResourceBindings());
        m_gridJacobiBindings[0]->setBindings({
            QRhiShaderResourceBinding::imageLoad(0, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0),
            QRhiShaderResourceBinding::imageLoad(1, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0), // divergence stored here
            QRhiShaderResourceBinding::imageStore(2, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[1].get(), 0)
        });
        m_gridJacobiBindings[0]->create();

        m_gridJacobiBindings[1].reset(m_rhi->newShaderResourceBindings());
        m_gridJacobiBindings[1]->setBindings({
            QRhiShaderResourceBinding::imageLoad(0, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[1].get(), 0),
            QRhiShaderResourceBinding::imageLoad(1, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0), // divergence stays in pressure[0]
            QRhiShaderResourceBinding::imageStore(2, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0)
        });
        m_gridJacobiBindings[1]->create();

        // Subtract pressure: vel, pressure (use pressure[0] as final)
        m_gridSubtractBindings.reset(m_rhi->newShaderResourceBindings());
        m_gridSubtractBindings->setBindings({
            QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_gridVel.get(), 0),
            QRhiShaderResourceBinding::imageLoad(1, QRhiShaderResourceBinding::ComputeStage, m_gridPressure[0].get(), 0)
        });
        m_gridSubtractBindings->create();
    }
}

void MpmRenderer::createParticleBuffer()
{
    m_particles.resize(m_particleCount);
    const QSize canvas = m_viewSize.isValid() ? m_viewSize : QSize(1024, 1024);
    QRandomGenerator rng(1);
    for (int i = 0; i < m_particleCount; ++i) {
        const float x = rng.generateDouble() * float(canvas.width());
        const float y = rng.generateDouble() * float(canvas.height());
        const float vx = (rng.generateDouble() - 0.5f) * 10.0f;
        const float vy = (rng.generateDouble() - 0.5f) * 10.0f;
        const float hue = float(i) / float(m_particleCount);
        const QColor c = QColor::fromHslF(hue, 0.75, 0.5);
        ParticleGpu p {};
        p.pos = QVector4D(x, y, 0.0f, float(c.blueF()));
        p.velColor = QVector4D(vx, vy, float(c.redF()), float(c.greenF()));
        m_particles[i] = p;
    }

    const qsizetype bufferSize = qsizetype(m_particles.size() * sizeof(ParticleGpu));
    m_particleBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::StorageBuffer, bufferSize));
    m_particleBuffer->create();
    m_particlesPendingUpload = true;
}

void MpmRenderer::ensurePipelines()
{
    if (!m_sampler) {
        m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        m_sampler->create();
    }

    ensureCanvas();
    ensureGrid();

    if (!m_viewParamsBuf) {
        m_viewParamsBuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D)));
        m_viewParamsBuf->create();
    }
    if (!m_simParamsBuf) {
        m_simParamsBuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D)));
        m_simParamsBuf->create();
    }
    if (!m_cohesionBuf) {
        m_cohesionBuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D)));
        m_cohesionBuf->create();
    }
    if (!m_blurParamsBuf) {
        m_blurParamsBuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D)));
        m_blurParamsBuf->create();
    }

    if (!m_splatBindings) {
        m_splatBindings.reset(m_rhi->newShaderResourceBindings());
        m_splatBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_viewParamsBuf.get())
        });
        m_splatBindings->create();
    }

    for (int i = 0; i < 2; ++i) {
        if (!m_presentBindings[i] && m_sampler && m_canvasTex[i]) {
            m_presentBindings[i].reset(m_rhi->newShaderResourceBindings());
            m_presentBindings[i]->setBindings({
                QRhiShaderResourceBinding::sampler(0, QRhiShaderResourceBinding::FragmentStage, m_sampler.get()),
                QRhiShaderResourceBinding::texture(1, QRhiShaderResourceBinding::FragmentStage, m_canvasTex[i].get())
            });
            m_presentBindings[i]->create();
        }
        if (!m_blurBindings[i] && m_sampler && m_canvasTex[i]) {
            m_blurBindings[i].reset(m_rhi->newShaderResourceBindings());
            m_blurBindings[i]->setBindings({
                QRhiShaderResourceBinding::sampler(0, QRhiShaderResourceBinding::FragmentStage, m_sampler.get()),
                QRhiShaderResourceBinding::texture(1, QRhiShaderResourceBinding::FragmentStage, m_canvasTex[i].get()),
                QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, m_blurParamsBuf.get())
            });
            m_blurBindings[i]->create();
        }
        if (!m_blurComputeBindings[i] && m_sampler && m_canvasTex[i]) {
            int srcIdx = 1 - i;
            if (!m_canvasTex[srcIdx])
                continue;
            m_blurComputeBindings[i].reset(m_rhi->newShaderResourceBindings());
            m_blurComputeBindings[i]->setBindings({
                QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_canvasTex[i].get(), 0),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::ComputeStage, m_canvasTex[srcIdx].get(), m_sampler.get()),
                QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::ComputeStage, m_blurParamsBuf.get())
            });
            m_blurComputeBindings[i]->create();
        }
    }

    if (!m_particlesP2GBindings && m_particleBuffer && m_simParamsBuf && m_gridVel && m_gridMass) {
        m_particlesP2GBindings.reset(m_rhi->newShaderResourceBindings());
        m_particlesP2GBindings->setBindings({
            QRhiShaderResourceBinding::bufferLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_particleBuffer.get()),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::ComputeStage, m_simParamsBuf.get()),
            QRhiShaderResourceBinding::imageLoadStore(2, QRhiShaderResourceBinding::ComputeStage, m_gridVel.get(), 0),
            QRhiShaderResourceBinding::imageLoadStore(3, QRhiShaderResourceBinding::ComputeStage, m_gridMass.get(), 0)
        });
        m_particlesP2GBindings->create();
    }
    if (!m_particlesG2PBindings && m_particleBuffer && m_simParamsBuf && m_gridVel && m_gridMass && m_gridPressure[0] && m_cohesionBuf) {
        m_particlesG2PBindings.reset(m_rhi->newShaderResourceBindings());
        m_particlesG2PBindings->setBindings({
            QRhiShaderResourceBinding::bufferLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_particleBuffer.get()),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::ComputeStage, m_simParamsBuf.get()),
            QRhiShaderResourceBinding::imageLoad(2, QRhiShaderResourceBinding::ComputeStage, m_gridVel.get(), 0),
            QRhiShaderResourceBinding::imageLoad(3, QRhiShaderResourceBinding::ComputeStage, m_gridMass.get(), 0),
            QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::ComputeStage, m_cohesionBuf.get())
        });
        m_particlesG2PBindings->create();
    }

    if (!m_splatPipeline && m_canvasRp[0]) {
        m_splatPipeline.reset(m_rhi->newGraphicsPipeline());
        m_splatPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_splatPipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_splatPipeline->setDepthTest(false);
        m_splatPipeline->setDepthWrite(false);
        m_splatPipeline->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/shaders/shaders/splat.vert.qsb"))},
            {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/shaders/shaders/splat.frag.qsb"))},
        });

        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        m_splatPipeline->setTargetBlends({blend});

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            QRhiVertexInputBinding(sizeof(QVector4D), QRhiVertexInputBinding::PerInstance)
        });
        inputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float4, 0)
        });

        m_splatPipeline->setVertexInputLayout(inputLayout);
        m_splatPipeline->setShaderResourceBindings(m_splatBindings.get());
        m_splatPipeline->setRenderPassDescriptor(m_canvasRp[0].get());
        m_splatPipeline->create();
    }

    if (!m_presentPipeline && m_presentBindings[0]) {
        m_presentPipeline.reset(m_rhi->newGraphicsPipeline());
        m_presentPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_presentPipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_presentPipeline->setDepthTest(false);
        m_presentPipeline->setDepthWrite(false);
        m_presentPipeline->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/shaders/shaders/fullscreen.vert.qsb"))},
            {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/shaders/shaders/fullscreen.frag.qsb"))},
        });
        QRhiVertexInputLayout inputLayout;
        m_presentPipeline->setVertexInputLayout(inputLayout);
        m_presentPipeline->setShaderResourceBindings(m_presentBindings[0].get());
        m_presentPipeline->setRenderPassDescriptor(m_swapChainPassDesc.get());
        m_presentPipeline->create();
    }

    if (!m_blurPipeline && m_blurBindings[0] && m_canvasRp[0]) {
        m_blurPipeline.reset(m_rhi->newGraphicsPipeline());
        m_blurPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_blurPipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_blurPipeline->setDepthTest(false);
        m_blurPipeline->setDepthWrite(false);
        m_blurPipeline->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/shaders/shaders/fullscreen.vert.qsb"))},
            {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/shaders/shaders/blur.frag.qsb"))},
        });
        QRhiVertexInputLayout inputLayout;
        m_blurPipeline->setVertexInputLayout(inputLayout);
        m_blurPipeline->setShaderResourceBindings(m_blurBindings[0].get());
        m_blurPipeline->setRenderPassDescriptor(m_canvasRp[0].get());
        m_blurPipeline->create();
    }

    if (!m_blurComputePipeline && m_blurComputeBindings[0] && m_blurComputeBindings[1]) {
        m_blurComputePipeline.reset(m_rhi->newComputePipeline());
        m_blurComputePipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/blur.comp.qsb"))});
        m_blurComputePipeline->setShaderResourceBindings(m_blurComputeBindings[0].get());
        if (!m_blurComputePipeline->create()) {
            qDebug() << "[compute] blur compute pipeline creation failed, using graphics blur";
            m_blurComputePipeline.reset();
        }
    }

    if (!m_particlesP2GPipeline && m_particlesP2GBindings) {
        m_particlesP2GPipeline.reset(m_rhi->newComputePipeline());
        m_particlesP2GPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/particles_p2g.comp.qsb"))});
        m_particlesP2GPipeline->setShaderResourceBindings(m_particlesP2GBindings.get());
        if (!m_particlesP2GPipeline->create()) {
            qDebug() << "[compute] particles P2G pipeline creation failed; fallback to CPU.";
            m_particlesP2GPipeline.reset();
        }
    }
    if (!m_particlesG2PPipeline && m_particlesG2PBindings) {
        m_particlesG2PPipeline.reset(m_rhi->newComputePipeline());
        m_particlesG2PPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/particles_g2p.comp.qsb"))});
        m_particlesG2PPipeline->setShaderResourceBindings(m_particlesG2PBindings.get());
        if (!m_particlesG2PPipeline->create()) {
            qDebug() << "[compute] particles G2P pipeline creation failed; fallback to CPU.";
            m_particlesG2PPipeline.reset();
        }
    }
    if (!m_particlesSphPipeline && m_gridMass && m_gridVel && m_particleBuffer && m_simParamsBuf) {
        m_particlesSphPipeline.reset(m_rhi->newComputePipeline());
        m_particlesSphPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/particles_sph.comp.qsb"))});
        // Reuse P2G bindings for mass/vel and particles, cohesion buffer can be omitted here.
        m_particlesSphPipeline->setShaderResourceBindings(m_particlesP2GBindings.get());
        if (!m_particlesSphPipeline->create()) {
            qDebug() << "[compute] particles SPH pipeline creation failed.";
            m_particlesSphPipeline.reset();
        }
    }
    if (!m_gridDivergencePipeline && m_gridDivergenceBindings) {
        m_gridDivergencePipeline.reset(m_rhi->newComputePipeline());
        m_gridDivergencePipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_divergence.comp.qsb"))});
        m_gridDivergencePipeline->setShaderResourceBindings(m_gridDivergenceBindings.get());
        if (!m_gridDivergencePipeline->create()) {
            qDebug() << "[compute] grid divergence pipeline creation failed.";
            m_gridDivergencePipeline.reset();
        }
    }
    if (!m_gridJacobiPipeline && m_gridPressure[0] && m_gridPressure[1]) {
        m_gridJacobiPipeline.reset(m_rhi->newComputePipeline());
        m_gridJacobiPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_jacobi.comp.qsb"))});
        if (!m_gridJacobiPipeline->create()) {
            qDebug() << "[compute] grid jacobi pipeline creation failed.";
            m_gridJacobiPipeline.reset();
        }
    }
    if (!m_gridSubtractPressurePipeline && m_gridSubtractBindings) {
        m_gridSubtractPressurePipeline.reset(m_rhi->newComputePipeline());
        m_gridSubtractPressurePipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_subtract_pressure.comp.qsb"))});
        if (!m_gridSubtractPressurePipeline->create()) {
            qDebug() << "[compute] grid subtract pressure pipeline creation failed.";
            m_gridSubtractPressurePipeline.reset();
        }
    }

    if (!m_gridClearPipeline && m_gridBindings) {
        m_gridClearPipeline.reset(m_rhi->newComputePipeline());
        m_gridClearPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_clear.comp.qsb"))});
        m_gridClearPipeline->setShaderResourceBindings(m_gridBindings.get());
        if (!m_gridClearPipeline->create()) {
            qDebug() << "[compute] grid clear pipeline creation failed.";
            m_gridClearPipeline.reset();
        }
    }
    if (!m_gridNormalizePipeline && m_gridBindings) {
        m_gridNormalizePipeline.reset(m_rhi->newComputePipeline());
        m_gridNormalizePipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_normalize.comp.qsb"))});
        m_gridNormalizePipeline->setShaderResourceBindings(m_gridBindings.get());
        if (!m_gridNormalizePipeline->create()) {
            qDebug() << "[compute] grid normalize pipeline creation failed.";
            m_gridNormalizePipeline.reset();
        }
    }
    if (!m_gridSmoothPipeline && m_gridBindings) {
        m_gridSmoothPipeline.reset(m_rhi->newComputePipeline());
        m_gridSmoothPipeline->setShaderStage({QRhiShaderStage::Compute, loadShader(QStringLiteral(":/shaders/shaders/grid_smooth.comp.qsb"))});
        m_gridSmoothPipeline->setShaderResourceBindings(m_gridBindings.get());
        if (!m_gridSmoothPipeline->create()) {
            qDebug() << "[compute] grid smooth pipeline creation failed.";
            m_gridSmoothPipeline.reset();
        }
    }
}

void MpmRenderer::updateParticlesCpu(float dtSeconds)
{
    Q_UNUSED(dtSeconds);
}

void MpmRenderer::renderFrame(float dtSeconds)
{
    if (!ensureSwapChain())
        return;

    if (!m_particleBuffer)
        createParticleBuffer();

    ensurePipelines();

    bool useComputeBlur = m_blurComputePipeline && m_blurComputeBindings[0] && m_blurComputeBindings[1];
    if (!m_splatPipeline || !m_presentPipeline || (!m_blurPipeline && !useComputeBlur)) {
        static bool logged = false;
        if (!logged) {
            qDebug() << "[render] pipelines missing"
                     << (m_splatPipeline != nullptr)
                     << (m_presentPipeline != nullptr)
                     << (m_blurPipeline != nullptr || useComputeBlur);
            logged = true;
        }
        return;
    }

    QRhiResourceUpdateBatch *rub = nullptr;
    QRhiCommandBuffer *cb = beginFrame(rub);
    if (!cb)
        return;

    const QSize canvasSize = m_canvasTex[0] ? m_canvasTex[0]->pixelSize() : m_viewSize;
    const QVector4D viewParams(float(canvasSize.width()), float(canvasSize.height()), 4.0f, 0.0f);   // radius in z
    rub->updateDynamicBuffer(m_viewParamsBuf.get(), 0, sizeof(QVector4D), &viewParams);
    // Clamp dt to avoid explosions.
    float clampedDt = std::clamp(dtSeconds, 0.0f, 0.010f);
    const QVector4D simParams(float(canvasSize.width()), float(canvasSize.height()), clampedDt, 40.0f);
    const QVector4D cohesionParams(m_cohesion, 0.0f, 0.0f, 0.0f);
    rub->updateDynamicBuffer(m_simParamsBuf.get(), 0, sizeof(QVector4D), &simParams);
    rub->updateDynamicBuffer(m_cohesionBuf.get(), 0, sizeof(QVector4D), &cohesionParams);

    const qsizetype bufferSizeParticles = qsizetype(m_particles.size() * sizeof(ParticleGpu));
    rub->updateDynamicBuffer(m_particleBuffer.get(), 0, bufferSizeParticles, m_particles.data());

    // If compute path available, run P2G -> normalize -> smooth -> pressure -> G2P.
    if (m_particlesP2GPipeline && m_particlesG2PPipeline && m_gridBindings) {
        if (m_gridClearPipeline) {
            cb->beginComputePass(rub);
            cb->setComputePipeline(m_gridClearPipeline.get());
            cb->setShaderResources(m_gridBindings.get());
            cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
            cb->endComputePass();
            rub = m_rhi->nextResourceUpdateBatch();
        }

        rub->updateDynamicBuffer(m_simParamsBuf.get(), 0, sizeof(QVector4D), &simParams);

        cb->beginComputePass(rub);
        cb->setComputePipeline(m_particlesP2GPipeline.get());
        cb->setShaderResources(m_particlesP2GBindings.get());
        const int groups = (m_particleCount + 63) / 64;
        cb->dispatch(groups, 1, 1);
        cb->endComputePass();
        rub = m_rhi->nextResourceUpdateBatch();

        if (m_gridNormalizePipeline) {
            cb->beginComputePass(rub);
            cb->setComputePipeline(m_gridNormalizePipeline.get());
            cb->setShaderResources(m_gridBindings.get());
            cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
            cb->endComputePass();
            rub = m_rhi->nextResourceUpdateBatch();
        }

        if (m_gridSmoothPipeline) {
            cb->beginComputePass(rub);
            cb->setComputePipeline(m_gridSmoothPipeline.get());
            cb->setShaderResources(m_gridBindings.get());
            cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
            cb->endComputePass();
            rub = m_rhi->nextResourceUpdateBatch();
        }

        // Pressure solve
        if (m_gridDivergencePipeline && m_gridJacobiPipeline && m_gridSubtractPressurePipeline && m_gridPressure[0] && m_gridPressure[1]) {
            // divergence into pressure[0]
            cb->beginComputePass(rub);
            cb->setComputePipeline(m_gridDivergencePipeline.get());
            cb->setShaderResources(m_gridDivergenceBindings.get());
            cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
            cb->endComputePass();
            rub = m_rhi->nextResourceUpdateBatch();

            // Jacobi iterations ping-pong between pressure[0] and pressure[1]
            bool toggle = false;
            for (int i = 0; i < m_pressureIterations; ++i) {
                cb->beginComputePass(rub);
                cb->setComputePipeline(m_gridJacobiPipeline.get());
                QRhiShaderResourceBindings *bindings = toggle ? m_gridJacobiBindings[1].get() : m_gridJacobiBindings[0].get();
                cb->setShaderResources(bindings);
                cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
                cb->endComputePass();
                rub = m_rhi->nextResourceUpdateBatch();
                toggle = !toggle;
            }

            cb->beginComputePass(rub);
            cb->setComputePipeline(m_gridSubtractPressurePipeline.get());
            cb->setShaderResources(m_gridSubtractBindings.get());
            cb->dispatch((m_gridSize.width() + 7) / 8, (m_gridSize.height() + 7) / 8, 1);
            cb->endComputePass();
            rub = m_rhi->nextResourceUpdateBatch();
        }

        cb->beginComputePass(rub);
        cb->setComputePipeline(m_particlesG2PPipeline.get());
        cb->setShaderResources(m_particlesG2PBindings.get());
        cb->dispatch((m_particleCount + 63) / 64, 1, 1);
        cb->endComputePass();
        rub = m_rhi->nextResourceUpdateBatch();
    }
    // Apply mouse impulses: spawn new paint with velocity.
    const auto impulses = m_input.take();
    if (!impulses.empty()) {
        QRandomGenerator rng(QDateTime::currentMSecsSinceEpoch());
        for (const auto &imp : impulses) {
            for (int n = 0; n < 16; ++n) {
                const int idx = rng.bounded(m_particleCount);
                auto &p = m_particles[idx];
                p.pos.setX(imp.pos.x());
                p.pos.setY(imp.pos.y());
                QVector2D jitter((rng.generateDouble() - 0.5) * 140.0, (rng.generateDouble() - 0.5) * 140.0);
                QVector2D vel = QVector2D(imp.vel) * 6.0f + jitter;
                p.velColor.setX(vel.x());
                p.velColor.setY(vel.y());
                const float hue = rng.generateDouble();
                const QColor c = QColor::fromHslF(hue, 0.85, 0.55);
                p.pos.setW(float(c.blueF()));
                p.velColor.setZ(float(c.redF()));
                p.velColor.setW(float(c.greenF()));
            }
        }
        // Upload updated particles immediately for GPU path if compute is active.
        const qsizetype bufferSize = qsizetype(m_particles.size() * sizeof(ParticleGpu));
        rub->updateDynamicBuffer(m_particleBuffer.get(), 0, bufferSize, m_particles.data());
    }

    if (!m_instanceBuf) {
        const qsizetype bufferSize = qsizetype(m_particles.size() * sizeof(QVector4D));
        m_instanceBuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, bufferSize));
        m_instanceBuf->create();
    }

    {
    std::vector<QVector4D> inst(m_particles.size());
    for (qsizetype i = 0; i < m_particles.size(); ++i) {
        const auto &p = m_particles[i];
        inst[i] = QVector4D(p.pos.x(), p.pos.y(), p.velColor.z(), p.velColor.w()); // pos.xy, color RG (B in pos.w)
    }
        const qsizetype bufferSize = qsizetype(inst.size() * sizeof(QVector4D));
        rub->updateDynamicBuffer(m_instanceBuf.get(), 0, bufferSize, inst.data());
    }

    // Blur pass: prefer compute, fall back to graphics.
    int srcIdx = m_canvasIndex;
    int dstIdx = 1 - srcIdx;
    QVector4D blurParamsH(1.0f / std::max(1, canvasSize.width()), 0.0f, 0.90f, 0.0f);
    QVector4D blurParamsV(0.0f, 1.0f / std::max(1, canvasSize.height()), 0.90f, 0.0f);

    if (useComputeBlur) {
        if (!m_loggedBlurBackend) {
            qDebug() << "[blur] using compute backend";
            m_loggedBlurBackend = true;
        }
        rub->updateDynamicBuffer(m_blurParamsBuf.get(), 0, sizeof(QVector4D), &blurParamsH);
        cb->beginComputePass(rub);
        cb->setComputePipeline(m_blurComputePipeline.get());
        cb->setShaderResources(m_blurComputeBindings[dstIdx].get()); // dst=dstIdx, src=srcIdx
        cb->dispatch((canvasSize.width() + 7) / 8, (canvasSize.height() + 7) / 8, 1);
        cb->endComputePass();

        rub = m_rhi->nextResourceUpdateBatch();
        rub->updateDynamicBuffer(m_blurParamsBuf.get(), 0, sizeof(QVector4D), &blurParamsV);
        cb->beginComputePass(rub);
        cb->setComputePipeline(m_blurComputePipeline.get());
        cb->setShaderResources(m_blurComputeBindings[srcIdx].get()); // dst=srcIdx, src=dstIdx
        cb->dispatch((canvasSize.width() + 7) / 8, (canvasSize.height() + 7) / 8, 1);
        cb->endComputePass();
    } else {
        rub->updateDynamicBuffer(m_blurParamsBuf.get(), 0, sizeof(QVector4D), &blurParamsH);
        cb->beginPass(m_canvasRt[dstIdx].get(), QColor(0, 0, 0, 0), {1.0f, 0}, rub);
        cb->setGraphicsPipeline(m_blurPipeline.get());
        cb->setShaderResources(m_blurBindings[srcIdx].get());
        cb->setViewport(QRhiViewport(0, 0, float(canvasSize.width()), float(canvasSize.height())));
        cb->draw(3);
        cb->endPass();

        rub = m_rhi->nextResourceUpdateBatch();
        rub->updateDynamicBuffer(m_blurParamsBuf.get(), 0, sizeof(QVector4D), &blurParamsV);
        cb->beginPass(m_canvasRt[srcIdx].get(), QColor(0, 0, 0, 0), {1.0f, 0}, rub);
        cb->setGraphicsPipeline(m_blurPipeline.get());
        cb->setShaderResources(m_blurBindings[dstIdx].get());
        cb->setViewport(QRhiViewport(0, 0, float(canvasSize.width()), float(canvasSize.height())));
        cb->draw(3);
        cb->endPass();
    }

    // Splat new paint onto blurred canvas (preserve existing content)
    rub = m_rhi->nextResourceUpdateBatch();
    QRhiCommandBuffer::BeginPassFlags splatFlags;
    QColor clearColor(0, 0, 0, 0);
    if (!m_canvasInitialized) {
        m_canvasInitialized = true;
    } else {
        splatFlags = QRhiCommandBuffer::ExternalContent;
    }

    cb->beginPass(m_canvasRt[srcIdx].get(), clearColor, {1.0f, 0}, rub, splatFlags);
    cb->setGraphicsPipeline(m_splatPipeline.get());
    cb->setShaderResources(m_splatBindings.get());
    cb->setViewport(QRhiViewport(0, 0, float(canvasSize.width()), float(canvasSize.height())));
    QRhiCommandBuffer::VertexInput vb[] = {
        { m_instanceBuf.get(), 0 }
    };
    cb->setVertexInput(0, 1, vb);
    cb->draw(24, m_particleCount);
    cb->endPass();

    const int presentIdx = srcIdx;
    m_canvasIndex = srcIdx;

    rub = m_rhi->nextResourceUpdateBatch();
    cb->beginPass(m_swapChain->currentFrameRenderTarget(), QColor(10, 10, 14), {1.0f, 0}, rub);
    cb->setGraphicsPipeline(m_presentPipeline.get());
    cb->setShaderResources(m_presentBindings[presentIdx].get());
    cb->setViewport(QRhiViewport(0, 0,
                                 float(m_swapChain->currentPixelSize().width()),
                                 float(m_swapChain->currentPixelSize().height())));
    cb->draw(3);
    cb->endPass();

    m_rhi->endFrame(m_swapChain.get());
}

QShader MpmRenderer::loadShader(const QString &resourcePath) const
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly))
        return QShader();
    const QByteArray data = f.readAll();
    return QShader::fromSerialized(data);
}

void MpmRenderer::onImpulse(const QPointF &pos, const QPointF &vel, float strength)
{
    m_input.push(pos, vel, strength);
}

QRhiCommandBuffer *MpmRenderer::beginFrame(QRhiResourceUpdateBatch *&rub)
{
    rub = m_rhi->nextResourceUpdateBatch();
    if (m_rhi->beginFrame(m_swapChain.get()) != QRhi::FrameOpSuccess) {
        rub = nullptr;
        return nullptr;
    }
    return m_swapChain->currentFrameCommandBuffer();
}
void MpmRenderer::shutdown()
{
    m_instanceBuf.reset();
    m_particleBuffer.reset();
    m_viewParamsBuf.reset();
    m_blurParamsBuf.reset();

    for (int i = 0; i < 2; ++i) {
        m_canvasRt[i].reset();
        m_canvasRp[i].reset();
        m_canvasTex[i].reset();
        m_presentBindings[i].reset();
        m_blurBindings[i].reset();
        m_blurComputeBindings[i].reset();
    }

    m_splatBindings.reset();
    m_presentPipeline.reset();
    m_splatPipeline.reset();
    m_blurPipeline.reset();
    m_blurComputePipeline.reset();
    m_sampler.reset();
    m_swapChain.reset();
    m_swapChainPassDesc.reset();
    m_depthStencil.reset();
    m_rhi.reset();
    m_fallbackSurface.reset();
}
