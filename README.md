# Restaurant Finder
# Overview
The Restaurant Finder is a C++ application that leverages the Google Places API to fetch and display detailed information about nearby restaurants. The application allows users to input a location (latitude and longitude) and a search radius, and it retrieves a list of restaurants within that area. It provides detailed information such as opening hours, ratings, cuisine types, and more. The data is then categorized and stored in maps for easy access and filtering.

# Features
1. Nearby Restaurant Search: Fetches restaurants within a specified radius from a given location.

2. Detailed Restaurant Information: Retrieves detailed information including opening hours, ratings, cuisine types, and operational status.

3. Categorization: Restaurants are categorized by type and rating for easy filtering.

4. Pagination Support: Handles pagination to fetch more than 20 results (up to 60).

5. Error Handling: Robust error handling for API requests and JSON parsing.

# Prerequisites
1. C++ Compiler: Ensure you have a C++ compiler installed (e.g., g++).

2. Boost Library: The application uses the Boost Property Tree library for JSON parsing. Install Boost if you don't have it already.

3. cURL: The application uses cURL for making HTTP requests to the Google Places API.

4. Google Places API Key: You need a valid API key from Google Cloud Platform to use the Google Places API.

# Installation
1. Clone the Repository:

git clone https://github.com/yourusername/restaurant-finder.git
cd restaurant-finder

2. Install Dependencies:

Boost: Install Boost using your package manager. 

3. Set Up API Key:

Create a .env file in the root directory of the project.

Add your Google Places API key to the .env file:

API_KEY=your_api_key_here

