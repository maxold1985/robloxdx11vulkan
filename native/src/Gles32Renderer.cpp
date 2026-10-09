#if defined(ROBLOX_HAS_GLES32)

#include "Gles32Renderer.h"
#include "EngineLog.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

namespace
{
	struct Vec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct Mat4
	{
		float m[16] = {};
	};

	struct Vertex
	{
		float px;
		float py;
		float pz;

		float nx;
		float ny;
		float nz;
	};

	float dot(
		const Vec3& a,
		const Vec3& b
	)
	{
		return
			a.x * b.x +
			a.y * b.y +
			a.z * b.z;
	}

	Vec3 subtract(
		const Vec3& a,
		const Vec3& b
	)
	{
		return {
			a.x - b.x,
			a.y - b.y,
			a.z - b.z
		};
	}

	Vec3 cross(
		const Vec3& a,
		const Vec3& b
	)
	{
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	Vec3 normalize(
		const Vec3& value
	)
	{
		const float lengthSquared =
			dot(
				value,
				value
			);

		if (
			lengthSquared <=
			1.0e-20f
		)
		{
			return {};
		}

		const float inverseLength =
			1.0f /
			std::sqrt(
				lengthSquared
			);

		return {
			value.x * inverseLength,
			value.y * inverseLength,
			value.z * inverseLength
		};
	}

	Mat4 identity()
	{
		Mat4 result;

		result.m[0] = 1.0f;
		result.m[5] = 1.0f;
		result.m[10] = 1.0f;
		result.m[15] = 1.0f;

		return result;
	}

	Mat4 multiply(
		const Mat4& a,
		const Mat4& b
	)
	{
		Mat4 result;

		for (
			int column = 0;
			column < 4;
			++column
		)
		{
			for (
				int row = 0;
				row < 4;
				++row
			)
			{
				float value = 0.0f;

				for (
					int k = 0;
					k < 4;
					++k
				)
				{
					value +=
						a.m[
							k * 4 +
							row
						] *
						b.m[
							column * 4 +
							k
						];
				}

				result.m[
					column * 4 +
					row
				] = value;
			}
		}

		return result;
	}

	Mat4 scaling(
		float x,
		float y,
		float z
	)
	{
		Mat4 result = identity();

		result.m[0] = x;
		result.m[5] = y;
		result.m[10] = z;

		return result;
	}

	Mat4 translation(
		float x,
		float y,
		float z
	)
	{
		Mat4 result = identity();

		result.m[12] = x;
		result.m[13] = y;
		result.m[14] = z;

		return result;
	}

	Mat4 perspective(
		float fovYRadians,
		float aspect,
		float nearPlane,
		float farPlane
	)
	{
		Mat4 result;

		if (aspect <= 0.0f)
			aspect = 1.0f;

		const float f =
			1.0f /
			std::tan(
				fovYRadians *
				0.5f
			);

		result.m[0] =
			f / aspect;

		result.m[5] = f;

		result.m[10] =
			(farPlane + nearPlane) /
			(nearPlane - farPlane);

		result.m[11] = -1.0f;

		result.m[14] =
			(2.0f *
				farPlane *
				nearPlane) /
			(nearPlane - farPlane);

		return result;
	}

	Mat4 lookAt(
		const Vec3& eye,
		const Vec3& target,
		const Vec3& up
	)
	{
		const Vec3 forward =
			normalize(
				subtract(
					target,
					eye
				)
			);

		const Vec3 side =
			normalize(
				cross(
					forward,
					up
				)
			);

		const Vec3 correctedUp =
			cross(
				side,
				forward
			);

		Mat4 result = identity();

		result.m[0] = side.x;
		result.m[4] = side.y;
		result.m[8] = side.z;

		result.m[1] = correctedUp.x;
		result.m[5] = correctedUp.y;
		result.m[9] = correctedUp.z;

		result.m[2] = -forward.x;
		result.m[6] = -forward.y;
		result.m[10] = -forward.z;

		result.m[12] =
			-dot(
				side,
				eye
			);

		result.m[13] =
			-dot(
				correctedUp,
				eye
			);

		result.m[14] =
			dot(
				forward,
				eye
			);

		return result;
	}

	GLuint compileShader(
		GLenum type,
		const char* source
	)
	{
		const GLuint shader =
			glCreateShader(type);

		if (!shader)
			return 0;

		glShaderSource(
			shader,
			1,
			&source,
			nullptr
		);

		glCompileShader(shader);

		GLint compiled = GL_FALSE;

		glGetShaderiv(
			shader,
			GL_COMPILE_STATUS,
			&compiled
		);

		if (compiled == GL_TRUE)
			return shader;

		GLint logLength = 0;

		glGetShaderiv(
			shader,
			GL_INFO_LOG_LENGTH,
			&logLength
		);

		std::vector<char> log(
			static_cast<std::size_t>(
				std::max(
					1,
					logLength
				)
			)
		);

		glGetShaderInfoLog(
			shader,
			static_cast<GLsizei>(
				log.size()
			),
			nullptr,
			log.data()
		);

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"GLES shader compile failed: %s",
			log.data()
		);

		glDeleteShader(shader);
		return 0;
	}

	GLuint createProgram()
	{
		static const char* vertexSource =
			"#version 320 es\n"
			"precision highp float;\n"
			"layout(location=0) in vec3 aPosition;\n"
			"layout(location=1) in vec3 aNormal;\n"
			"uniform mat4 uWorld;\n"
			"uniform mat4 uViewProjection;\n"
			"out vec3 vNormal;\n"
			"void main()\n"
			"{\n"
			"    vec4 worldPosition = uWorld * vec4(aPosition, 1.0);\n"
			"    gl_Position = uViewProjection * worldPosition;\n"
			"    vNormal = normalize(mat3(uWorld) * aNormal);\n"
			"}\n";

		static const char* fragmentSource =
			"#version 320 es\n"
			"precision highp float;\n"
			"in vec3 vNormal;\n"
			"uniform vec4 uColor;\n"
			"layout(location=0) out vec4 outColor;\n"
			"void main()\n"
			"{\n"
			"    vec3 lightDirection = normalize(vec3(-0.4, 0.8, -0.6));\n"
			"    float lighting = max(dot(normalize(vNormal), lightDirection), 0.0);\n"
			"    lighting = 0.35 + lighting * 0.65;\n"
			"    outColor = vec4(uColor.rgb * lighting, uColor.a);\n"
			"}\n";

		const GLuint vertex =
			compileShader(
				GL_VERTEX_SHADER,
				vertexSource
			);

		if (!vertex)
			return 0;

		const GLuint fragment =
			compileShader(
				GL_FRAGMENT_SHADER,
				fragmentSource
			);

		if (!fragment)
		{
			glDeleteShader(vertex);
			return 0;
		}

		const GLuint program =
			glCreateProgram();

		glAttachShader(
			program,
			vertex
		);

		glAttachShader(
			program,
			fragment
		);

		glLinkProgram(program);

		glDeleteShader(vertex);
		glDeleteShader(fragment);

		GLint linked = GL_FALSE;

		glGetProgramiv(
			program,
			GL_LINK_STATUS,
			&linked
		);

		if (linked == GL_TRUE)
			return program;

		GLint logLength = 0;

		glGetProgramiv(
			program,
			GL_INFO_LOG_LENGTH,
			&logLength
		);

		std::vector<char> log(
			static_cast<std::size_t>(
				std::max(
					1,
					logLength
				)
			)
		);

		glGetProgramInfoLog(
			program,
			static_cast<GLsizei>(
				log.size()
			),
			nullptr,
			log.data()
		);

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"GLES program link failed: %s",
			log.data()
		);

		glDeleteProgram(program);
		return 0;
	}

	std::string keyFromX11(
		KeySym symbol
	)
	{
		switch (symbol)
		{
		case XK_w:
		case XK_W:
			return "W";

		case XK_a:
		case XK_A:
			return "A";

		case XK_s:
		case XK_S:
			return "S";

		case XK_d:
		case XK_D:
			return "D";

		case XK_space:
			return "Space";

		default:
			return {};
		}
	}

	bool versionAtLeast32(
		const char* version
	)
	{
		if (!version)
			return false;

		int major = 0;
		int minor = 0;

		const char* marker =
			std::strstr(
				version,
				"OpenGL ES "
			);

		if (!marker)
			return false;

		marker +=
			std::strlen(
				"OpenGL ES "
			);

		if (
			std::sscanf(
				marker,
				"%d.%d",
				&major,
				&minor
			) != 2
		)
		{
			return false;
		}

		return
			major > 3 ||
			(
				major == 3 &&
				minor >= 2
			);
	}

	constexpr Vertex CUBE_VERTICES[] = {
		{-0.5f,-0.5f,-0.5f, 0.0f, 0.0f,-1.0f},
		{ 0.5f,-0.5f,-0.5f, 0.0f, 0.0f,-1.0f},
		{ 0.5f, 0.5f,-0.5f, 0.0f, 0.0f,-1.0f},
		{-0.5f, 0.5f,-0.5f, 0.0f, 0.0f,-1.0f},

		{-0.5f,-0.5f, 0.5f, 0.0f, 0.0f, 1.0f},
		{ 0.5f,-0.5f, 0.5f, 0.0f, 0.0f, 1.0f},
		{ 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f},
		{-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f},

		{-0.5f,-0.5f,-0.5f,-1.0f, 0.0f, 0.0f},
		{-0.5f, 0.5f,-0.5f,-1.0f, 0.0f, 0.0f},
		{-0.5f, 0.5f, 0.5f,-1.0f, 0.0f, 0.0f},
		{-0.5f,-0.5f, 0.5f,-1.0f, 0.0f, 0.0f},

		{ 0.5f,-0.5f,-0.5f, 1.0f, 0.0f, 0.0f},
		{ 0.5f, 0.5f,-0.5f, 1.0f, 0.0f, 0.0f},
		{ 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f},
		{ 0.5f,-0.5f, 0.5f, 1.0f, 0.0f, 0.0f},

		{-0.5f,-0.5f,-0.5f, 0.0f,-1.0f, 0.0f},
		{-0.5f,-0.5f, 0.5f, 0.0f,-1.0f, 0.0f},
		{ 0.5f,-0.5f, 0.5f, 0.0f,-1.0f, 0.0f},
		{ 0.5f,-0.5f,-0.5f, 0.0f,-1.0f, 0.0f},

		{-0.5f, 0.5f,-0.5f, 0.0f, 1.0f, 0.0f},
		{-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f},
		{ 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f},
		{ 0.5f, 0.5f,-0.5f, 0.0f, 1.0f, 0.0f},
	};

	constexpr std::uint16_t CUBE_INDICES[] = {
		 0,  2,  1,  0,  3,  2,
		 4,  5,  6,  4,  6,  7,
		 8,  9, 10,  8, 10, 11,
		12, 14, 13, 12, 15, 14,
		16, 18, 17, 16, 19, 18,
		20, 21, 22, 20, 22, 23,
	};
}

struct Gles32Renderer::Impl
{
	Display* xDisplay = nullptr;
	Window xWindow = 0;
	Colormap xColormap = 0;
	Atom wmDelete = 0;
	EGLint nativeVisualId = 0;

	EGLDisplay eglDisplay =
		EGL_NO_DISPLAY;

	EGLConfig eglConfig = nullptr;

	EGLSurface eglSurface =
		EGL_NO_SURFACE;

	EGLContext eglContext =
		EGL_NO_CONTEXT;

	int width = 1280;
	int height = 720;
	bool running = false;

	InputCallback inputCallback;

	GLuint program = 0;

	GLuint cubeVao = 0;
	GLuint cubeVbo = 0;
	GLuint cubeIbo = 0;

	GLuint gridVao = 0;
	GLuint gridVbo = 0;
	GLsizei gridVertexCount = 0;

	GLint worldLocation = -1;
	GLint viewProjectionLocation = -1;
	GLint colorLocation = -1;

	bool createDisplayAndConfig()
	{
		xDisplay =
			XOpenDisplay(nullptr);

		if (!xDisplay)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"XOpenDisplay failed; verify DISPLAY/Termux:X11"
			);

			return false;
		}

		eglDisplay =
			eglGetDisplay(
				reinterpret_cast<
					EGLNativeDisplayType
				>(
					xDisplay
				)
			);

		if (
			eglDisplay ==
			EGL_NO_DISPLAY
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglGetDisplay failed"
			);

			return false;
		}

		EGLint eglMajor = 0;
		EGLint eglMinor = 0;

		if (
			eglInitialize(
				eglDisplay,
				&eglMajor,
				&eglMinor
			) != EGL_TRUE
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglInitialize failed"
			);

			return false;
		}

		if (
			eglBindAPI(
				EGL_OPENGL_ES_API
			) != EGL_TRUE
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglBindAPI OpenGL ES failed"
			);

			return false;
		}

		const EGLint configAttributes[] = {
			EGL_SURFACE_TYPE,
			EGL_WINDOW_BIT,

			EGL_RENDERABLE_TYPE,
			EGL_OPENGL_ES3_BIT_KHR,

			EGL_RED_SIZE,
			8,

			EGL_GREEN_SIZE,
			8,

			EGL_BLUE_SIZE,
			8,

			EGL_ALPHA_SIZE,
			8,

			EGL_DEPTH_SIZE,
			24,

			EGL_NONE
		};

		EGLint configCount = 0;

		if (
			eglChooseConfig(
				eglDisplay,
				configAttributes,
				&eglConfig,
				1,
				&configCount
			) != EGL_TRUE ||
			configCount < 1
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglChooseConfig GLES3 failed"
			);

			return false;
		}

		if (
			eglGetConfigAttrib(
				eglDisplay,
				eglConfig,
				EGL_NATIVE_VISUAL_ID,
				&nativeVisualId
			) != EGL_TRUE
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglGetConfigAttrib EGL_NATIVE_VISUAL_ID failed"
			);

			return false;
		}

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"EGL %d.%d visual=%d",
			eglMajor,
			eglMinor,
			nativeVisualId
		);

		return true;
	}

	bool createX11Window(
		const char* title
	)
	{
		if (!xDisplay)
			return false;

		const int screen =
			DefaultScreen(xDisplay);

		XVisualInfo visualTemplate{};
		visualTemplate.visualid =
			static_cast<VisualID>(
				nativeVisualId
			);
		visualTemplate.screen =
			screen;

		int visualCount = 0;

		XVisualInfo* visualInfo =
			XGetVisualInfo(
				xDisplay,
				VisualIDMask |
					VisualScreenMask,
				&visualTemplate,
				&visualCount
			);

		if (
			!visualInfo ||
			visualCount < 1
		)
		{
			if (visualInfo)
				XFree(visualInfo);

			EngineLog::write(
				EngineLog::Component::Renderer,
				"XGetVisualInfo for EGL visual failed"
			);

			return false;
		}

		const Window root =
			RootWindow(
				xDisplay,
				screen
			);

		xColormap =
			XCreateColormap(
				xDisplay,
				root,
				visualInfo->visual,
				AllocNone
			);

		XSetWindowAttributes attributes{};
		attributes.colormap =
			xColormap;
		attributes.event_mask =
			ExposureMask |
			KeyPressMask |
			KeyReleaseMask |
			StructureNotifyMask;

		xWindow =
			XCreateWindow(
				xDisplay,
				root,
				0,
				0,
				static_cast<unsigned int>(
					width
				),
				static_cast<unsigned int>(
					height
				),
				0,
				visualInfo->depth,
				InputOutput,
				visualInfo->visual,
				CWColormap |
					CWEventMask,
				&attributes
			);

		XFree(visualInfo);

		if (!xWindow)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"XCreateWindow failed"
			);

			return false;
		}

		XStoreName(
			xDisplay,
			xWindow,
			title
				? title
				: "Roblox GLES 3.2"
		);

		wmDelete =
			XInternAtom(
				xDisplay,
				"WM_DELETE_WINDOW",
				False
			);

		XSetWMProtocols(
			xDisplay,
			xWindow,
			&wmDelete,
			1
		);

		XMapWindow(
			xDisplay,
			xWindow
		);

		XFlush(xDisplay);

		return true;
	}

	bool createEglContext()
	{
		eglSurface =
			eglCreateWindowSurface(
				eglDisplay,
				eglConfig,
				static_cast<
					EGLNativeWindowType
				>(
					xWindow
				),
				nullptr
			);

		if (
			eglSurface ==
			EGL_NO_SURFACE
		)
		{
			EngineLog::writef(
				EngineLog::Component::Renderer,
				"eglCreateWindowSurface failed: 0x%04x",
				static_cast<unsigned int>(
					eglGetError()
				)
			);

			return false;
		}

#if defined(EGL_CONTEXT_MAJOR_VERSION_KHR) && defined(EGL_CONTEXT_MINOR_VERSION_KHR)
		const EGLint contextAttributes[] = {
			EGL_CONTEXT_MAJOR_VERSION_KHR,
			3,
			EGL_CONTEXT_MINOR_VERSION_KHR,
			2,
			EGL_NONE
		};
#else
		const EGLint contextAttributes[] = {
			EGL_CONTEXT_CLIENT_VERSION,
			3,
			EGL_NONE
		};
#endif

		eglContext =
			eglCreateContext(
				eglDisplay,
				eglConfig,
				EGL_NO_CONTEXT,
				contextAttributes
			);

		if (
			eglContext ==
			EGL_NO_CONTEXT
		)
		{
			EngineLog::writef(
				EngineLog::Component::Renderer,
				"eglCreateContext GLES failed: 0x%04x",
				static_cast<unsigned int>(
					eglGetError()
				)
			);

			return false;
		}

		if (
			eglMakeCurrent(
				eglDisplay,
				eglSurface,
				eglSurface,
				eglContext
			) != EGL_TRUE
		)
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"eglMakeCurrent failed"
			);

			return false;
		}

		const char* version =
			reinterpret_cast<
				const char*
			>(
				glGetString(
					GL_VERSION
				)
			);

		const char* renderer =
			reinterpret_cast<
				const char*
			>(
				glGetString(
					GL_RENDERER
				)
			);

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"GLES version: %s",
			version
				? version
				: "unknown"
		);

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"GLES renderer: %s",
			renderer
				? renderer
				: "unknown"
		);

		if (!versionAtLeast32(version))
		{
			EngineLog::write(
				EngineLog::Component::Renderer,
				"OpenGL ES 3.2 required"
			);

			return false;
		}

		eglSwapInterval(
			eglDisplay,
			1
		);

		return true;
	}

	bool createResources()
	{
		program =
			createProgram();

		if (!program)
			return false;

		worldLocation =
			glGetUniformLocation(
				program,
				"uWorld"
			);

		viewProjectionLocation =
			glGetUniformLocation(
				program,
				"uViewProjection"
			);

		colorLocation =
			glGetUniformLocation(
				program,
				"uColor"
			);

		if (
			worldLocation < 0 ||
			viewProjectionLocation < 0 ||
			colorLocation < 0
		)
		{
			return false;
		}

		glGenVertexArrays(
			1,
			&cubeVao
		);

		glGenBuffers(
			1,
			&cubeVbo
		);

		glGenBuffers(
			1,
			&cubeIbo
		);

		glBindVertexArray(
			cubeVao
		);

		glBindBuffer(
			GL_ARRAY_BUFFER,
			cubeVbo
		);

		glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(CUBE_VERTICES),
			CUBE_VERTICES,
			GL_STATIC_DRAW
		);

		glBindBuffer(
			GL_ELEMENT_ARRAY_BUFFER,
			cubeIbo
		);

		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			sizeof(CUBE_INDICES),
			CUBE_INDICES,
			GL_STATIC_DRAW
		);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			reinterpret_cast<void*>(0)
		);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			reinterpret_cast<void*>(
				sizeof(float) * 3
			)
		);

		std::vector<Vertex>
			gridVertices;

		constexpr int gridHalfCount = 20;
		constexpr float gridSpacing = 4.0f;
		constexpr float gridExtent =
			gridHalfCount *
			gridSpacing;

		for (
			int line = -gridHalfCount;
			line <= gridHalfCount;
			++line
		)
		{
			const float offset =
				static_cast<float>(
					line
				) *
				gridSpacing;

			gridVertices.push_back(
				{
					-gridExtent,
					0.0f,
					offset,
					0.0f,
					1.0f,
					0.0f
				}
			);

			gridVertices.push_back(
				{
					gridExtent,
					0.0f,
					offset,
					0.0f,
					1.0f,
					0.0f
				}
			);

			gridVertices.push_back(
				{
					offset,
					0.0f,
					-gridExtent,
					0.0f,
					1.0f,
					0.0f
				}
			);

			gridVertices.push_back(
				{
					offset,
					0.0f,
					gridExtent,
					0.0f,
					1.0f,
					0.0f
				}
			);
		}

		gridVertexCount =
			static_cast<GLsizei>(
				gridVertices.size()
			);

		glGenVertexArrays(
			1,
			&gridVao
		);

		glGenBuffers(
			1,
			&gridVbo
		);

		glBindVertexArray(
			gridVao
		);

		glBindBuffer(
			GL_ARRAY_BUFFER,
			gridVbo
		);

		glBufferData(
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(
				gridVertices.size() *
				sizeof(Vertex)
			),
			gridVertices.data(),
			GL_STATIC_DRAW
		);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			reinterpret_cast<void*>(0)
		);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			reinterpret_cast<void*>(
				sizeof(float) * 3
			)
		);

		glBindVertexArray(0);

		glEnable(
			GL_DEPTH_TEST
		);

		glDepthFunc(
			GL_LEQUAL
		);

		glEnable(
			GL_BLEND
		);

		glBlendFunc(
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA
		);

		return true;
	}

	void destroy()
	{
		if (
			eglDisplay !=
			EGL_NO_DISPLAY &&
			eglContext !=
			EGL_NO_CONTEXT
		)
		{
			eglMakeCurrent(
				eglDisplay,
				eglSurface,
				eglSurface,
				eglContext
			);
		}

		if (gridVbo)
			glDeleteBuffers(
				1,
				&gridVbo
			);

		if (gridVao)
			glDeleteVertexArrays(
				1,
				&gridVao
			);

		if (cubeIbo)
			glDeleteBuffers(
				1,
				&cubeIbo
			);

		if (cubeVbo)
			glDeleteBuffers(
				1,
				&cubeVbo
			);

		if (cubeVao)
			glDeleteVertexArrays(
				1,
				&cubeVao
			);

		if (program)
			glDeleteProgram(program);

		program = 0;
		gridVbo = 0;
		gridVao = 0;
		cubeIbo = 0;
		cubeVbo = 0;
		cubeVao = 0;

		if (
			eglDisplay !=
			EGL_NO_DISPLAY
		)
		{
			eglMakeCurrent(
				eglDisplay,
				EGL_NO_SURFACE,
				EGL_NO_SURFACE,
				EGL_NO_CONTEXT
			);

			if (
				eglContext !=
				EGL_NO_CONTEXT
			)
			{
				eglDestroyContext(
					eglDisplay,
					eglContext
				);
			}

			if (
				eglSurface !=
				EGL_NO_SURFACE
			)
			{
				eglDestroySurface(
					eglDisplay,
					eglSurface
				);
			}

			eglTerminate(
				eglDisplay
			);
		}

		eglContext = EGL_NO_CONTEXT;
		eglSurface = EGL_NO_SURFACE;
		eglDisplay = EGL_NO_DISPLAY;

		if (
			xDisplay &&
			xWindow
		)
		{
			XDestroyWindow(
				xDisplay,
				xWindow
			);

			xWindow = 0;
		}

		if (
			xDisplay &&
			xColormap
		)
		{
			XFreeColormap(
				xDisplay,
				xColormap
			);

			xColormap = 0;
		}

		if (xDisplay)
		{
			XCloseDisplay(
				xDisplay
			);

			xDisplay = nullptr;
		}
	}
};

Gles32Renderer::Gles32Renderer()
	: impl_(
		std::make_unique<Impl>()
	)
{
}

Gles32Renderer::~Gles32Renderer()
{
	if (impl_)
		impl_->destroy();
}

bool Gles32Renderer::initialize(
	const char* title,
	int width,
	int height
)
{
	impl_->width =
		std::max(
			1,
			width
		);

	impl_->height =
		std::max(
			1,
			height
		);

	if (!impl_->createDisplayAndConfig())
		return false;

	if (
		!impl_->createX11Window(
			title
		)
	)
	{
		return false;
	}

	if (!impl_->createEglContext())
		return false;

	if (!impl_->createResources())
		return false;

	impl_->running = true;

	EngineLog::writef(
		EngineLog::Component::Renderer,
		"GLES 3.2 initialized %dx%d",
		impl_->width,
		impl_->height
	);

	return true;
}

void Gles32Renderer::setInputCallback(
	InputCallback callback
)
{
	impl_->inputCallback =
		std::move(callback);
}

bool Gles32Renderer::pumpEvents()
{
	if (
		!impl_->running ||
		!impl_->xDisplay
	)
	{
		return false;
	}

	while (
		XPending(
			impl_->xDisplay
		) > 0
	)
	{
		XEvent event{};
		XNextEvent(
			impl_->xDisplay,
			&event
		);

		if (
			event.type ==
			ClientMessage &&
			static_cast<Atom>(
				event.xclient.data.l[0]
			) ==
				impl_->wmDelete
		)
		{
			impl_->running = false;
			break;
		}

		if (
			event.type ==
			ConfigureNotify
		)
		{
			impl_->width =
				std::max(
					1,
					event.xconfigure.width
				);

			impl_->height =
				std::max(
					1,
					event.xconfigure.height
				);

			continue;
		}

		if (
			event.type ==
				KeyPress ||
			event.type ==
				KeyRelease
		)
		{
			const KeySym symbol =
				XLookupKeysym(
					&event.xkey,
					0
				);

			const std::string key =
				keyFromX11(
					symbol
				);

			if (
				!key.empty() &&
				impl_->inputCallback
			)
			{
				impl_->inputCallback(
					key,
					event.type ==
						KeyPress
				);
			}
		}
	}

	return impl_->running;
}

void Gles32Renderer::render(
	const std::vector<
		RobloxObjectModel::RenderPartSnapshot
	>& parts,
	const RobloxObjectModel::RenderCameraSnapshot&
		camera
)
{
	if (!impl_->running)
		return;

	glViewport(
		0,
		0,
		impl_->width,
		impl_->height
	);

	glClearColor(
		0.07f,
		0.10f,
		0.16f,
		1.0f
	);

	glClear(
		GL_COLOR_BUFFER_BIT |
		GL_DEPTH_BUFFER_BIT
	);

	glUseProgram(
		impl_->program
	);

	Vec3 target{};

	if (camera.hasSubject)
	{
		target = {
			camera.targetX,
			camera.targetY,
			camera.targetZ
		};
	}

	const Vec3 eye{
		target.x + 14.0f,
		target.y + 10.0f,
		target.z + 18.0f
	};

	const Mat4 view =
		lookAt(
			eye,
			target,
			{
				0.0f,
				1.0f,
				0.0f
			}
		);

	const float aspect =
		impl_->height > 0
			? static_cast<float>(
				impl_->width
			) /
				static_cast<float>(
					impl_->height
				)
			: 1.0f;

	const Mat4 projection =
		perspective(
			70.0f *
				3.14159265358979323846f /
				180.0f,
			aspect,
			0.1f,
			2000.0f
		);

	const Mat4 viewProjection =
		multiply(
			projection,
			view
		);

	glUniformMatrix4fv(
		impl_->viewProjectionLocation,
		1,
		GL_FALSE,
		viewProjection.m
	);

	const Mat4 gridWorld =
		identity();

	glUniformMatrix4fv(
		impl_->worldLocation,
		1,
		GL_FALSE,
		gridWorld.m
	);

	glUniform4f(
		impl_->colorLocation,
		0.22f,
		0.26f,
		0.32f,
		1.0f
	);

	glBindVertexArray(
		impl_->gridVao
	);

	glDrawArrays(
		GL_LINES,
		0,
		impl_->gridVertexCount
	);

	glBindVertexArray(
		impl_->cubeVao
	);

	for (
		const auto& part :
		parts
	)
	{
		if (
			part.transparency >=
			1.0f
		)
		{
			continue;
		}

		const Mat4 world =
			multiply(
				translation(
					part.positionX,
					part.positionY,
					part.positionZ
				),
				scaling(
					std::max(
						part.sizeX,
						0.001f
					),
					std::max(
						part.sizeY,
						0.001f
					),
					std::max(
						part.sizeZ,
						0.001f
					)
				)
			);

		glUniformMatrix4fv(
			impl_->worldLocation,
			1,
			GL_FALSE,
			world.m
		);

		glUniform4f(
			impl_->colorLocation,
			std::clamp(
				part.colorR,
				0.0f,
				1.0f
			),
			std::clamp(
				part.colorG,
				0.0f,
				1.0f
			),
			std::clamp(
				part.colorB,
				0.0f,
				1.0f
			),
			std::clamp(
				1.0f -
					part.transparency,
				0.0f,
				1.0f
			)
		);

		glDrawElements(
			GL_TRIANGLES,
			static_cast<GLsizei>(
				std::size(
					CUBE_INDICES
				)
			),
			GL_UNSIGNED_SHORT,
			nullptr
		);
	}

	glBindVertexArray(0);

	eglSwapBuffers(
		impl_->eglDisplay,
		impl_->eglSurface
	);
}

bool Gles32Renderer::isRunning() const
{
	return
		impl_ &&
		impl_->running;
}

#endif
