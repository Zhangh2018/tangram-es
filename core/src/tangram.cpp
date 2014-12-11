#include "tangram.h"

#include <memory>
#include <utility>
#include <cmath>
#include <time.h>

#include "platform.h"
#include "tile/tileManager.h"
#include "view/view.h"
#include "data/dataSource.h"

#include "style/polygonStyle.h"
#include "style/polylineStyle.h"
#include "scene/scene.h"
#include "scene/lights.h"
#include "util/error.h"

namespace Tangram {

std::unique_ptr<TileManager> m_tileManager;    
std::shared_ptr<Scene> m_scene;
std::shared_ptr<View> m_view;

void initialize() {
    
    logMsg("%s\n", "initialize");

    // Create view
    if (!m_view) {
        m_view = std::make_shared<View>();
        
        // Move the view to coordinates in Manhattan so we have something interesting to test
        glm::dvec2 target = m_view->getMapProjection().LonLatToMeters(glm::dvec2(-74.00796, 40.70361));
        m_view->setPosition(target.x, target.y);
    }

    // Create a scene object
    if (!m_scene) {
        m_scene = std::make_shared<Scene>();
        
        // Load style(s); hard-coded for now
        std::unique_ptr<Style> polyStyle(new PolygonStyle("Polygon"));
        polyStyle->addLayers({
            "buildings",
            "water",
            "earth",
            "landuse"
        });
        m_scene->addStyle(std::move(polyStyle));
        
        std::unique_ptr<Style> linesStyle(new PolylineStyle("Polyline"));
        linesStyle->addLayers({"roads"});
        m_scene->addStyle(std::move(linesStyle));

        //------ TESTING LIGHTS

        //  Directional
        // DirectionalLight* dlight = new DirectionalLight();
        // dlight->setDirection(glm::vec3(-1.0, -1.0, 1.0));
        // std::unique_ptr<Light> directionLight(dlight);
        // m_scene->addLight(std::move(directionalLight));
    
        //  Point
        PointLight * pLight = new PointLight();
        pLight->setDiffuseColor(glm::vec4(0.0,1.0,0.0,1.0));
        pLight->setSpecularColor(glm::vec4(0.5,0.0,1.0,1.0));
        pLight->setAttenuation(0.0,0.01);
        pLight->setPosition(glm::vec3(0.0));
        std::shared_ptr<Light> pointLight(pLight);
        m_scene->addLight(pointLight);

        //  Spot
        SpotLight * sLight = new SpotLight();
        sLight->setSpecularColor(glm::vec4(0.5,0.5,0.0,1.0));
        sLight->setPosition(glm::vec3(0.0));
        sLight->setDirection(glm::vec3(0,PI*0.25,0.0));
        sLight->setCutOff(PI*0.1, 0.2);
        sLight->setAttenuation(0.0,0.02);
        std::unique_ptr<Light> spotLight(sLight);
        m_scene->addLight(std::move(spotLight));
        
        //-----------------------

        m_scene->buildShaders();
    }

    // Create a tileManager
    if (!m_tileManager) {
        m_tileManager = TileManager::GetInstance();
        
        // Pass references to the view and scene into the tile manager
        m_tileManager->setView(m_view);
        m_tileManager->setScene(m_scene);
        
        // Add a tile data source
        std::unique_ptr<DataSource> dataSource(new MapzenVectorTileJson());
        m_tileManager->addDataSource(std::move(dataSource));
    }

    // Set up openGL state
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
    glClearDepthf(1.0);
    glDepthRangef(0.0, 1.0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    while (Error::hadGlError("Tangram::initialize()")) {}

    logMsg("%s\n", "finish initialize");
}

void resize(int _newWidth, int _newHeight) {
    
    logMsg("%s\n", "resize");

    glViewport(0, 0, _newWidth, _newHeight);

    if (m_view) {
        m_view->setSize(_newWidth, _newHeight);
    }

    while (Error::hadGlError("Tangram::resize()")) {}

}

void update(float _dt) {

    if (m_tileManager) {
        m_tileManager->updateTileSet();
    }
    
    if(m_scene){
        //------ TESTING LIGHTS
        //
        float time = ((float)clock())/CLOCKS_PER_SEC;
        for(int i = 0 ; i < m_scene->getLights().size(); i++){

            if(m_scene->getLights()[i]->getType() == LIGHT_POINT){
                PointLight* tmp = dynamic_cast<PointLight*>( (m_scene->getLights()[i]).get() );
                tmp->setPosition(glm::vec3( 100*cos(time),
                                            100*sin(time), 
                                            -m_view->getPosition().z+100));
            } else if(m_scene->getLights()[i]->getType() == LIGHT_SPOT){
                SpotLight* tmp = dynamic_cast<SpotLight*>( (m_scene->getLights()[i]).get() );
                tmp->setDirection(glm::vec3(cos(time),
                                            sin(time), 
                                            0.0));
                tmp->setPosition(glm::vec3(0.0, 0.0, -m_view->getPosition().z+100));
            } 
        }
    }   
}

void render() {

    // Set up openGL for new frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Loop over all styles
    for (const auto& style : m_scene->getStyles()) {

        style->setup();

        // Loop over visible tiles
        for (const auto& mapIDandTile : m_tileManager->getVisibleTiles()) {

            const std::unique_ptr<MapTile>& tile = mapIDandTile.second;
            
            if (tile) {
                // Draw!
//                tile->draw(*style, viewProj);
                
                //  Can we pass only the scene?
                //
                tile->draw(*m_scene, *style, *m_view);
            }

        }
    }

    while (Error::hadGlError("Tangram::render()")) {}

}
    
void handleTapGesture(float _posX, float _posY) {
    logMsg("Do tap: (%f,%f)\n", _posX, _posY);
    m_view->translate(_posX, _posY);
}

void handleDoubleTapGesture(float _posX, float _posY) {
    logMsg("Do double tap: (%f,%f)\n", _posX, _posY);
}

void handlePanGesture(float _velX, float _velY) {
    // Scaled with reference to 16 zoom level
    float invZoomScale = 0.1 * pow(2,(16 - m_view->getZoom()));
    m_view->translate(-_velX * invZoomScale, _velY * invZoomScale);
    logMsg("Pan Velocity: (%f,%f)\n", _velX, _velY);
}

void handlePinchGesture(float _posX, float _posY, float _scale) {
    logMsg("Do pinch, pos1: (%f, %f)\tscale: (%f)\n", _posX, _posY, _scale);
    m_view->zoom( _scale < 1.0 ? -1 : 1);
}

void teardown() {
    // TODO: Release resources!
}

void onContextDestroyed() {
    
    // The OpenGL context has been destroyed since the last time resources were created,
    // so we invalidate all data that depends on OpenGL object handles.

    // ShaderPrograms are invalidated and immediately rebuilt
    ShaderProgram::invalidateAllPrograms();

    // Buffer objects are invalidated and re-uploaded the next time they are used
    VboMesh::invalidateAllVBOs();
    
}
    
}
