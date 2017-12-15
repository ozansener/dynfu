#include <dynfu/utils/opt_solver.hpp>

CombinedSolver::CombinedSolver(Warpfield warpfield, CombinedSolverParameters params) {
    m_warpfield                = warpfield;
    m_combinedSolverParameters = params;
}

void CombinedSolver::initializeProblemInstance(const std::shared_ptr<dynfu::Frame> canonicalFrame,
                                               const std::shared_ptr<dynfu::Frame> liveFrame) {
    m_canonicalVerticesOpenCV = canonicalFrame->getVertices();
    m_canonicalNormalsOpenCV  = canonicalFrame->getNormals();
    m_liveVerticesOpenCV      = liveFrame->getVertices();
    m_liveNormalsOpenCV       = liveFrame->getNormals();

    unsigned int D = m_warpfield.getNodes().size();
    unsigned int N = m_canonicalVerticesOpenCV.size();

    m_dims = {D, N};

    m_canonicalVertices = createEmptyOptImage({N}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);
    m_canonicalNormals  = createEmptyOptImage({N}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);
    m_liveVertices      = createEmptyOptImage({N}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);
    m_liveNormals       = createEmptyOptImage({N}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);

    m_rotation    = createEmptyOptImage({D}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);
    m_translation = createEmptyOptImage({D}, OptImage::Type::FLOAT, 3, OptImage::GPU, true);

    m_transformationWeights = createEmptyOptImage({N}, OptImage::Type::FLOAT, 8, OptImage::GPU, true);

    resetGPUMemory();
    initializeConnectivity(m_canonicalVerticesOpenCV);
    addOptSolvers(m_dims, std::string("/homes/dig15/df/dynfu/include/dynfu/utils/terra/energy.t"));
}

void CombinedSolver::initializeConnectivity(std::vector<cv::Vec3f> canonicalVertices) {
    int N = canonicalVertices.size();

    std::vector<std::vector<int>> indices(9, std::vector<int>(N));
    std::vector<std::array<float, 8>> transformationWeights(N);
    std::array<float, 8> transformationWeightsArray{};

    for (int count = 0; count < canonicalVertices.size(); count++) {
        indices[0].push_back(count);

        auto vertexNeighbours    = m_warpfield.findNeighbors(8, canonicalVertices[count]);
        auto vertexNeighboursIdx = m_warpfield.findNeighborsIndex(8, canonicalVertices[count]);

        for (int i = 1; i < indices.size(); i++) {
            transformationWeightsArray[i - 1] =
                vertexNeighbours[i - 1]->getTransformationWeight(canonicalVertices[count]);
            indices[i].push_back(vertexNeighboursIdx[i - 1]);
        }

        transformationWeights.push_back(transformationWeightsArray);
    }

    m_transformationWeights->update(transformationWeights);
    m_dataGraph = std::make_shared<OptGraph>(indices);
}

void CombinedSolver::combinedSolveInit() {
    m_solverParams.set("nIterations", &m_combinedSolverParameters.nonLinearIter);
    m_solverParams.set("lIterations", &m_combinedSolverParameters.linearIter);

    m_problemParams.set("nodeCoordinates", m_nodeCoordinates);

    m_problemParams.set("rotation", m_rotation);
    m_problemParams.set("translation", m_translation);
    m_problemParams.set("radialBasisWeights", m_radialBasisWeights);

    m_problemParams.set("canonicalVertices", m_canonicalVertices);
    m_problemParams.set("canonicalNormals", m_canonicalNormals);

    m_problemParams.set("liveVertices", m_liveVertices);
    m_problemParams.set("liveNormals", m_liveNormals);

    m_problemParams.set("dataGraph", m_dataGraph);

}

void CombinedSolver::preSingleSolve() {}

void CombinedSolver::postSingleSolve() { copyResultToCPUFromFloat3(); }

void CombinedSolver::preNonlinearSolve(int /* iteration */) {}

void CombinedSolver::postNonlinearSolve(int /* iteration */) {}

void CombinedSolver::combinedSolveFinalize() {
    reportFinalCosts("warp field optimisation", m_combinedSolverParameters, getCost("Opt(GN)"), getCost("Opt(LM)"),
                     nan(""));
}

void CombinedSolver::resetGPUMemory() {
    auto N = m_dims[1];

    std::vector<float3> h_canonicalVertices(N);
    std::vector<float3> h_canonicalNormals(N);

    std::vector<float3> h_liveVertices(N);
    std::vector<float3> h_liveNormals(N);

    for (int i = 0; i < N; i++) {
        h_canonicalVertices[i] = make_float3(m_canonicalVerticesOpenCV[i][0], m_canonicalVerticesOpenCV[i][1],
                                             m_canonicalVerticesOpenCV[i][2]);
        h_canonicalNormals[i] =
            make_float3(m_canonicalNormalsOpenCV[i][0], m_canonicalNormalsOpenCV[i][1], m_canonicalNormalsOpenCV[i][2]);

        h_liveVertices[i] =
            make_float3(m_liveVerticesOpenCV[i][0], m_liveVerticesOpenCV[i][1], m_liveVerticesOpenCV[i][2]);
        h_liveNormals[i] = make_float3(m_liveNormalsOpenCV[i][0], m_liveNormalsOpenCV[i][1], m_liveNormalsOpenCV[i][2]);
    }

    m_canonicalVertices->update(h_canonicalVertices);
    m_canonicalNormals->update(h_canonicalNormals);

    m_liveVertices->update(h_liveVertices);
    m_liveNormals->update(h_liveNormals);

    auto D = m_dims[0];

    std::vector<float3> h_translation(D);
    std::vector<float3> h_rotation(D);

    for (int i = 0; i < D; i++) {
        auto nodeTransformation = m_warpfield.getNodes()[i]->getTransformation();
        auto nodeTranslation    = nodeTransformation->getTranslation();
        auto nodeRotation       = nodeTransformation->getRotation();

        h_translation[i] = make_float3(nodeTranslation[0], nodeTranslation[1], nodeTranslation[2]);
        h_rotation[i]    = make_float3(0.f, 0.f, 0.f);  // FIXME (dig15): set the rotations
    }

    m_translation->update(h_translation);
    m_rotation->update(h_rotation);
}

void CombinedSolver::copyResultToCPUFromFloat3() {
    auto D = m_dims[0];
    std::vector<float3> h_translation(D);

    m_translation->copyTo(h_translation);

    for (unsigned int i = 0; i < D; i++) {
        m_warpfield.getNodes()[i]->setTranslation(
            cv::Vec3f(h_translation[i].x, h_translation[i].y, h_translation[i].z));
    }
}
