#include <SDL.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cmath>

const unsigned int WIDTH = 256;
const unsigned int HEIGHT = 128;

struct FPoint {
	float x;
	float y;
	float u;
	float v;
};

void polyfill(std::vector< unsigned char >& pixels, const std::vector<FPoint>& poly) {
	int ymin = INT_MAX;
	int ymax = INT_MIN;
	for (auto it = poly.begin();it < poly.end(); it++ ) {
		ymin = std::min(ymin, (int)floor(it->y));
		ymax = std::max(ymax, (int)floor(it->y));
	}
	std::vector<std::pair<bool, FPoint>> spans(ymax - ymin + 1, { false, {} });

 	int n = poly.size();
	for (int i = 0;i < n;++i) {
		FPoint p0{ poly[i] };
		FPoint p1{ poly[(i + 1) % n] };
		if (p0.y > p1.y) {
			const FPoint tmp{ p0 };
			p0 = p1 ;
			p1 = tmp;
		}
		float dy = p1.y - p0.y;
		float dx = (p1.x - p0.x) / dy;
		float du = (p1.u - p0.u) / dy;
		float dv = (p1.v - p0.v) / dy;
		if (p0.y < 0) {
			p0.x -= p0.y * dx;
			p0.u -= p0.y * du;
			p0.v -= p0.y * dv;
			p0.y = 0;
		}
		float x = p0.x;
		float u = p0.u;
		float v = p0.v;
		// sub-pixel starter
		float suby = (floor(p0.y) - p0.y + 1);
		x += dx * suby;
		u += du * suby;
		v += dv * suby;
		for (int y = (int)floor(p0.y);y<std::min((int)HEIGHT,(int)floor(p1.y));++y) {
			// start of span?
			const auto& span = spans[y - ymin];
			if (span.first) {
				// fill row
				FPoint x0{ span.second };
				FPoint x1{ x, y, u, v };
				if (x0.x > x1.x) {
					const FPoint tmp(x0);
					x0 = x1;
					x1 = tmp;
				}
				const int baseOffset = (WIDTH * 4 * y);
				float ddx = x1.x - x0.x;
				float uu = x0.u;
				float vv = x0.v;
				float ddu = (x1.u - x0.u) / ddx;
				float ddv = (x1.v - x0.v) / ddx;
				float subx = (floor(x0.x) - x0.x + 1);
				uu += ddu * subx;
				vv += ddv * subx;
				for (int xx = (int)x0.x;xx < std::min((int)WIDTH, (int)x1.x);++xx) {
					const unsigned int offset = baseOffset + xx * 4;
					pixels[offset + 0] = 0;        // b
					pixels[offset + 1] = (int)(64 * floor(4 * uu));        // g
					pixels[offset + 2] = (int)(64 * floor(4 * vv));        // r
					pixels[offset + 3] = SDL_ALPHA_OPAQUE;    // a

					uu += ddu;
					vv += ddv;
				}
			}
			else
			{
				spans[y - ymin] = { true, {x,(float)y,u,v} };
			}
			x += dx;
			u += du;
			v += dv;
		}
	}
}

FPoint rotate(const FPoint& p, const FPoint& center, float angle) {
	const FPoint tmp{ p.x - center.x, p.y - center.y };
	const float cs = cos(angle);
	const float ss = sin(angle);
	return {
		center.x + cs * tmp.x + ss * tmp.y,
		center.y + ss * tmp.x - cs * tmp.y,
		p.u, p.v
	};
}

int main(int argc, char* argv[]) {
	using std::cerr;
	using std::endl;

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		cerr << "SDL_Init Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}


	SDL_Window* win = SDL_CreateWindow("*Engine*", 100, 100, 4*WIDTH, 4*HEIGHT, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == nullptr) {
		cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
		if (win != nullptr) SDL_DestroyWindow(win);
		return EXIT_FAILURE;
	}

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	std::cout << "Renderer name: " << info.name << std::endl;
	std::cout << "Texture formats: " << std::endl;
	for (Uint32 i = 0; i < info.num_texture_formats; i++)
	{
		std::cout << SDL_GetPixelFormatName(info.texture_formats[i]) << std::endl;
	}

	SDL_Texture* texture = SDL_CreateTexture
	(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		WIDTH, HEIGHT
	);
	
	std::vector< unsigned char > pixels(WIDTH * HEIGHT * 4, 0);

	SDL_Event event;
	bool running = true;
	bool useLocktexture = false;

	unsigned int frames = 0;
	int angle = 0;
	Uint64 start = SDL_GetPerformanceCounter();

	while (running) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		while (SDL_PollEvent(&event))
		{
			if ((SDL_QUIT == event.type) ||
				(SDL_KEYDOWN == event.type && SDL_SCANCODE_ESCAPE == event.key.keysym.scancode))
			{
				running = false;
				break;
			}
			if (SDL_KEYDOWN == event.type && SDL_SCANCODE_L == event.key.keysym.scancode)
			{
				useLocktexture = !useLocktexture;
				std::cout << "Using " << (useLocktexture ? "SDL_LockTexture() + memcpy()" : "SDL_UpdateTexture()") << std::endl;
			}
		}

		// clear tmp buffer
		pixels.assign(pixels.size(),0);
		// polyfill
		
		std::vector<FPoint> poly = {
			{32, 32, 0,0},
			{96, 32, 1,0},
			{96, 96, 1,1},
			{32, 96, 0, 1} };

		std::vector<FPoint> rotatedPoly{};
		FPoint center{ 64,64 };
		for (auto it = poly.begin();it < poly.end();++it) {
			rotatedPoly.push_back(rotate(*it, center, M_PI * angle / 360.0 / 32));
		}
		polyfill(pixels, rotatedPoly);
		angle++;

		if (useLocktexture)
		{
			unsigned char* lockedPixels = nullptr;
			int pitch = 0;
			SDL_LockTexture
			(
				texture,
				NULL,
				reinterpret_cast<void**>(&lockedPixels),
				&pitch
			);
			std::memcpy(lockedPixels, pixels.data(), pixels.size());
			SDL_UnlockTexture(texture);
		}
		else
		{
			SDL_UpdateTexture
			(
				texture,
				NULL,
				pixels.data(),
				WIDTH * 4
			);
		}

		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		frames++;
		const Uint64 end = SDL_GetPerformanceCounter();
		const static Uint64 freq = SDL_GetPerformanceFrequency();
		const double seconds = (end - start) / static_cast<double>(freq);
		if (seconds > 2.0)
		{
			std::cout
				<< frames << " frames in "
				<< std::setprecision(1) << std::fixed << seconds << " seconds = "
				<< std::setprecision(1) << std::fixed << frames / seconds << " FPS ("
				<< std::setprecision(3) << std::fixed << (seconds * 1000.0) / frames << " ms/frame)"
				<< std::endl;
			start = end;
			frames = 0;
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);

	return 0;
}