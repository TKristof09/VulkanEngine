float Bias_DirectionalLight(Surface surface, DirectionalLightInformation information) {
	return max(information.light.depthBias * (1.0f - dot(surface.normal, information.light.direction.xyz)), information.light.depthBias);
}

float HardShadows_DirectionalLight(sampler2DArray shadowMap, vec3 shadowCoords, int layer,
								   Surface surface, DirectionalLightInformation information, float shadowDarkness) {
	const float bias = Bias_DirectionalLight(surface, information);
	const float z = texture(shadowMap, vec3(shadowCoords.x, 1.0f - shadowCoords.y, float(layer))).r;

	if (step(z + bias, shadowCoords.z) > 0.5f) {
		return 1.0f - shadowDarkness * information.shadow.shadowFade;
	}

	return 1.0f;
}

vec2 SearchRegionRadiusUV_DirectionalLight(float zWorld, Cascade cascade) {
	const vec2 lightRadiusUV = vec2(0.02f); // TODO: from light size
	return lightRadiusUV * (zWorld - cascade.zNear) / zWorld;
}

vec2 PenumbraRadiusUV_DirectionalLight(float zReceiver, float zBlocker) {
	const vec2 lightRadiusUV = vec2(0.02f); // TODO: from light size
	return lightRadiusUV * (zReceiver - zBlocker) / zBlocker;
}

vec2 ProjectToLightUV_DirectionalLight(vec2 sizeUV, float zWorld, Cascade cascade) {
	return sizeUV * cascade.zNear / zWorld;
}

float PenumbraSize_DirectionalLight(float zReceiver, float zBlocker) {
	return (zReceiver - zBlocker) / zBlocker;
}

float SearchWidth_DirectionalLight(float uvLightSize, float receiverDistance, vec3 cameraPosition) {
	const float near = 0.1f;

	// return uvLightSize * (receiverDistance - near) / receiverDistance;
	return uvLightSize * (receiverDistance - near) / cameraPosition.z;
}

float ZClipToEye_DirectionalLight(float z, Cascade cascade) {
	return cascade.zFar * cascade.zNear / (cascade.zFar - z * (cascade.zFar - cascade.zNear));
}

float FindBlockerDistance_DirectionalLight(sampler2DArray shadowMap, vec3 shadowCoords, int layer,
										   Surface surface, DirectionalLightInformation information, float zEye) {
	const int numBlockerSearchSamples = POISSON_SAMPLE_SIZE;
	const float bias = Bias_DirectionalLight(surface, information);

	int blockers = 0;
	float avgBlockerDistance = 0;

	const vec2 searchWidth = SearchRegionRadiusUV_DirectionalLight(zEye, information.cascade);
	// const float searchWidth = SearchWidth_DirectionalLight(information.shadow.lightSize, shadowCoords.z, surface.cameraPosition);
	for (int i = 0; i < numBlockerSearchSamples; i++) {
		const vec2 possionedSample = samplePoisson(i) * searchWidth;
		const float z = texture(shadowMap, vec3(shadowCoords.x + possionedSample.x, 1.0f - shadowCoords.y + possionedSample.y, float(layer))).r;
		if (step(z + bias, shadowCoords.z) > 0.5f) {
			blockers++;
			avgBlockerDistance += z;
		}
	}

	if (blockers > 0) {
		return avgBlockerDistance / float(blockers);
	}

	return -1.0f;
}

float PCF_DirectionalLight(sampler2DArray shadowMap, vec3 shadowCoords, int layer,
						   vec2 uvRadius, Surface surface, DirectionalLightInformation information) {
	const int numPCFSamples = POISSON_SAMPLE_SIZE;
	const float bias = Bias_DirectionalLight(surface, information);

	// const ivec2 texDim = textureSize(shadowMap, 0).xy;
	// const float scale = 3.0f;
	// const vec2 size = 1.0f / vec2(texDim);

	float sum = 0.0f;
	for (int i = 0; i < numPCFSamples; i++) {
		const vec2 possionedSample = samplePoisson(i) * uvRadius;
		const float z = texture(shadowMap, vec3(shadowCoords.x + possionedSample.x, 1.0f - shadowCoords.y + possionedSample.y, float(layer))).r;

		sum += step(z + bias, shadowCoords.z);
	}

	return sum / float(numPCFSamples);
}

float PCSS_DirectionalLight(sampler2DArray shadowMap, vec3 shadowCoords, int layer,
							Surface surface, DirectionalLightInformation information, float shadowDarkness) {
	const float zEye = -(information.cascade.view * vec4(surface.worldPosition, 1.0f)).z;
	const float blockerDistance = FindBlockerDistance_DirectionalLight(shadowMap, shadowCoords, layer, surface, information, zEye);

	if (blockerDistance <= -1.0f) {
		return 1.0f;
	}

	const float avgBlockerDepthWorld = ZClipToEye_DirectionalLight(blockerDistance, information.cascade);
	const vec2 penumbraWidth = PenumbraRadiusUV_DirectionalLight(zEye, avgBlockerDepthWorld);
	const vec2 uvRadius = ProjectToLightUV_DirectionalLight(penumbraWidth, zEye, information.cascade);

	return 1.0f - PCF_DirectionalLight(shadowMap, shadowCoords, layer, uvRadius, surface, information) * shadowDarkness * information.shadow.shadowFade;
}
