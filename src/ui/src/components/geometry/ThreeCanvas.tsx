import React, { useEffect, useRef, useState, useCallback } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { StlBinaryMesh, CsvBinaryPoints } from '../../api/types';
import { buildPointsColors } from '../../core/colormap';

export interface InspectedNode {
  nodeId: number;
  x: number;
  y: number;
  z: number;
  temperature: number;
  index: number;
}

interface Props {
  mode: 'stl' | 'csv' | null;
  stlData: StlBinaryMesh | null;
  pointsData: CsvBinaryPoints | null;
  minTemp?: number;
  maxTemp?: number;
  pointSize: number;
  selectedNode: InspectedNode | null;
  onSelectNode: (node: InspectedNode | null) => void;
  fitTrigger: number;
  resetTrigger: number;
}

export const ThreeCanvas: React.FC<Props> = ({
  mode,
  stlData,
  pointsData,
  minTemp = 0,
  maxTemp = 100,
  pointSize,
  selectedNode,
  onSelectNode,
  fitTrigger,
  resetTrigger,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.PerspectiveCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const currentObjectRef = useRef<THREE.Object3D | null>(null);
  const highlightMeshRef = useRef<THREE.Mesh | null>(null);
  const raycasterRef = useRef<THREE.Raycaster>(new THREE.Raycaster());
  const mouseRef = useRef<THREE.Vector2>(new THREE.Vector2());
  const [hoveredNode, setHoveredNode] = useState<InspectedNode | null>(null);

  // Setup Three.js scene, camera, lights, controls
  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    const width = container.clientWidth || 800;
    const height = container.clientHeight || 600;

    // Scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x131418); // Deep graphite
    sceneRef.current = scene;

    // Subtle grid helper
    const grid = new THREE.GridHelper(10, 20, 0x3e434f, 0x22242a);
    grid.position.y = -0.01;
    scene.add(grid);

    // Camera
    const camera = new THREE.PerspectiveCamera(45, width / height, 0.01, 1000);
    camera.position.set(2, 2, 3);
    cameraRef.current = camera;

    // Renderer
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = false;
    container.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    // Controls
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.rotateSpeed = 0.8;
    controls.zoomSpeed = 1.0;
    controls.panSpeed = 0.8;
    controlsRef.current = controls;

    // Lights
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.7);
    scene.add(ambientLight);

    const dirLight1 = new THREE.DirectionalLight(0xffffff, 0.8);
    dirLight1.position.set(5, 10, 7);
    scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0x94a3b8, 0.4);
    dirLight2.position.set(-5, -5, -5);
    scene.add(dirLight2);

    // Node highlight marker
    const markerGeo = new THREE.SphereGeometry(1, 16, 16);
    const markerMat = new THREE.MeshBasicMaterial({
      color: 0x38bdf8,
      wireframe: true,
      depthTest: false,
    });
    const markerMesh = new THREE.Mesh(markerGeo, markerMat);
    markerMesh.visible = false;
    scene.add(markerMesh);
    highlightMeshRef.current = markerMesh;

    // Animation Loop
    let animId: number;
    const animate = () => {
      animId = requestAnimationFrame(animate);
      controls.update();
      renderer.render(scene, camera);
    };
    animate();

    // Resize Observer
    const resizeObserver = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const { width: w, height: h } = entry.contentRect;
        if (w > 0 && h > 0) {
          camera.aspect = w / h;
          camera.updateProjectionMatrix();
          renderer.setSize(w, h);
        }
      }
    });
    resizeObserver.observe(container);

    return () => {
      cancelAnimationFrame(animId);
      resizeObserver.disconnect();
      controls.dispose();
      renderer.dispose();
      if (renderer.domElement.parentElement) {
        renderer.domElement.parentElement.removeChild(renderer.domElement);
      }
    };
  }, []);

  // Fit to view function
  const fitToView = useCallback(() => {
    const object = currentObjectRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    if (!object || !camera || !controls) return;

    const box = new THREE.Box3().setFromObject(object);
    if (box.isEmpty()) return;

    const sphere = box.getBoundingSphere(new THREE.Sphere());
    const radius = Math.max(sphere.radius, 0.01);
    const center = sphere.center;

    controls.target.copy(center);

    const fov = camera.fov * (Math.PI / 180);
    const distance = (radius / Math.sin(fov / 2)) * 1.3;

    // Position camera along current direction
    const direction = camera.position.clone().sub(controls.target).normalize();
    if (direction.lengthSq() < 0.001) direction.set(0.6, 0.6, 0.8).normalize();

    camera.position.copy(center).addScaledVector(direction, distance);
    camera.near = radius * 0.01;
    camera.far = radius * 50;
    camera.updateProjectionMatrix();
    controls.update();
  }, []);

  // Reset view function
  const resetView = useCallback(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    if (!camera || !controls) return;

    controls.target.set(0, 0, 0);
    camera.position.set(2, 2, 3);
    camera.near = 0.01;
    camera.far = 1000;
    camera.updateProjectionMatrix();
    controls.update();
    fitToView();
  }, [fitToView]);

  // Listen to external fit/reset triggers
  useEffect(() => {
    if (fitTrigger > 0) fitToView();
  }, [fitTrigger, fitToView]);

  useEffect(() => {
    if (resetTrigger > 0) resetView();
  }, [resetTrigger, resetView]);

  // Keyboard shortcut 'F' for fit-to-view
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) {
        return; // Don't trigger if user is typing in input
      }
      if (e.key === 'f' || e.key === 'F') {
        fitToView();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [fitToView]);

  // Render STL or CSV data
  useEffect(() => {
    const scene = sceneRef.current;
    if (!scene) return;

    // Remove existing object
    if (currentObjectRef.current) {
      scene.remove(currentObjectRef.current);
      if (currentObjectRef.current instanceof THREE.Mesh || currentObjectRef.current instanceof THREE.Points) {
        currentObjectRef.current.geometry.dispose();
      }
      currentObjectRef.current = null;
    }

    if (highlightMeshRef.current) {
      highlightMeshRef.current.visible = false;
    }

    if (mode === 'stl' && stlData) {
      // Create STL Mesh
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute(
        'position',
        new THREE.BufferAttribute(stlData.positions, 3)
      );
      geometry.setAttribute(
        'normal',
        new THREE.BufferAttribute(stlData.normals, 3)
      );

      const material = new THREE.MeshStandardMaterial({
        color: 0x94a3b8,
        metalness: 0.15,
        roughness: 0.45,
        side: THREE.DoubleSide,
      });

      const mesh = new THREE.Mesh(geometry, material);
      scene.add(mesh);
      currentObjectRef.current = mesh;

      setTimeout(fitToView, 50);
    } else if (mode === 'csv' && pointsData) {
      // Create Points
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute(
        'position',
        new THREE.BufferAttribute(pointsData.xyz, 3)
      );

      const colors = buildPointsColors(pointsData.temperatures, minTemp, maxTemp);
      geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

      const material = new THREE.PointsMaterial({
        size: pointSize,
        vertexColors: true,
        sizeAttenuation: true,
      });

      const points = new THREE.Points(geometry, material);
      scene.add(points);
      currentObjectRef.current = points;

      setTimeout(fitToView, 50);
    }
  }, [mode, stlData, pointsData, minTemp, maxTemp, fitToView]);

  // Update point size
  useEffect(() => {
    if (mode === 'csv' && currentObjectRef.current instanceof THREE.Points) {
      const mat = currentObjectRef.current.material as THREE.PointsMaterial;
      mat.size = pointSize;
      mat.needsUpdate = true;
    }
  }, [mode, pointSize]);

  // Update highlight marker when selectedNode changes
  useEffect(() => {
    const marker = highlightMeshRef.current;
    if (!marker) return;

    if (selectedNode && mode === 'csv' && pointsData) {
      marker.position.set(selectedNode.x, selectedNode.y, selectedNode.z);
      // Size marker proportionally to pointSize
      const s = Math.max(0.015, pointSize * 0.8);
      marker.scale.set(s, s, s);
      marker.visible = true;
    } else {
      marker.visible = false;
    }
  }, [selectedNode, mode, pointsData, pointSize]);

  // Raycasting for point cloud node inspection (click & hover)
  const handlePointerDown = (e: React.PointerEvent<HTMLDivElement>) => {
    if (mode !== 'csv' || !pointsData || !currentObjectRef.current || !cameraRef.current) return;
    const container = containerRef.current;
    if (!container) return;

    const rect = container.getBoundingClientRect();
    mouseRef.current.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouseRef.current.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    const raycaster = raycasterRef.current;
    raycaster.setFromCamera(mouseRef.current, cameraRef.current);
    raycaster.params.Points.threshold = Math.max(0.03, pointSize * 0.5);

    const intersects = raycaster.intersectObject(currentObjectRef.current);
    if (intersects.length > 0 && intersects[0].index !== undefined) {
      const idx = intersects[0].index;
      const inspected: InspectedNode = {
        nodeId: pointsData.nodeIds[idx],
        x: pointsData.xyz[idx * 3],
        y: pointsData.xyz[idx * 3 + 1],
        z: pointsData.xyz[idx * 3 + 2],
        temperature: pointsData.temperatures[idx],
        index: idx,
      };
      onSelectNode(inspected);
    }
  };

  const handlePointerMove = (e: React.PointerEvent<HTMLDivElement>) => {
    if (mode !== 'csv' || !pointsData || !currentObjectRef.current || !cameraRef.current) {
      if (hoveredNode) setHoveredNode(null);
      return;
    }
    const container = containerRef.current;
    if (!container) return;

    const rect = container.getBoundingClientRect();
    mouseRef.current.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouseRef.current.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    const raycaster = raycasterRef.current;
    raycaster.setFromCamera(mouseRef.current, cameraRef.current);
    raycaster.params.Points.threshold = Math.max(0.03, pointSize * 0.5);

    const intersects = raycaster.intersectObject(currentObjectRef.current);
    if (intersects.length > 0 && intersects[0].index !== undefined) {
      const idx = intersects[0].index;
      setHoveredNode({
        nodeId: pointsData.nodeIds[idx],
        x: pointsData.xyz[idx * 3],
        y: pointsData.xyz[idx * 3 + 1],
        z: pointsData.xyz[idx * 3 + 2],
        temperature: pointsData.temperatures[idx],
        index: idx,
      });
    } else {
      if (hoveredNode) setHoveredNode(null);
    }
  };

  return (
    <div
      ref={containerRef}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      className="relative w-full h-full min-h-[400px] bg-canvas overflow-hidden cursor-grab active:cursor-grabbing select-none"
    >
      {/* 3D Viewport Controls Overlay */}
      <div className="absolute top-3 right-3 flex items-center gap-2 z-10 bg-panel/85 backdrop-blur border border-border-subtle p-1 rounded-md shadow-panel">
        <button
          onClick={fitToView}
          title="Fit to view (Hotkey: F)"
          className="px-2.5 py-1 text-xs font-mono text-text-secondary hover:text-text-primary hover:bg-subtle rounded border border-border-subtle flex items-center gap-1"
        >
          <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4" />
          </svg>
          <span>Fit <kbd className="text-[10px] bg-canvas px-1 rounded border border-border-subtle">F</kbd></span>
        </button>
        <button
          onClick={resetView}
          title="Reset camera view"
          className="px-2.5 py-1 text-xs font-mono text-text-secondary hover:text-text-primary hover:bg-subtle rounded border border-border-subtle"
        >
          Reset
        </button>
      </div>

      {/* Hover tooltip for Point Cloud */}
      {hoveredNode && mode === 'csv' && (
        <div className="absolute bottom-3 left-3 z-10 bg-panel/90 backdrop-blur border border-accent/40 text-text-primary text-xs font-mono p-2.5 rounded-md shadow-dropdown pointer-events-none tabular-nums">
          <div className="font-semibold text-accent mb-1 flex items-center justify-between gap-3">
            <span>Node #{hoveredNode.nodeId}</span>
            <span className="text-status-info">{hoveredNode.temperature.toFixed(2)} °C</span>
          </div>
          <div className="text-[11px] text-text-secondary grid grid-cols-3 gap-2">
            <span>x: {hoveredNode.x.toFixed(4)}m</span>
            <span>y: {hoveredNode.y.toFixed(4)}m</span>
            <span>z: {hoveredNode.z.toFixed(4)}m</span>
          </div>
        </div>
      )}

      {/* Orbit instructions banner */}
      <div className="absolute bottom-3 right-3 z-10 text-[10px] font-mono text-text-muted bg-panel/75 backdrop-blur px-2 py-1 rounded border border-border-subtle pointer-events-none hidden md:block">
        Left-drag: Rotate | Right-drag / Shift: Pan | Scroll: Zoom
      </div>
    </div>
  );
};

