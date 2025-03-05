Restaurant Finder
Overview
The Restaurant Finder is a C++ application that leverages the Google Places API to fetch and display detailed information about nearby restaurants. The application allows users to input a location (latitude and longitude) and a search radius, and it retrieves a list of restaurants within that area. It provides detailed information such as opening hours, ratings, cuisine types, and more. The data is then categorized and stored in maps for easy access and filtering.

Features
Nearby Restaurant Search: Fetches restaurants within a specified radius from a given location.

Detailed Restaurant Information: Retrieves detailed information including opening hours, ratings, cuisine types, and operational status.

Categorization: Restaurants are categorized by type and rating for easy filtering.

Pagination Support: Handles pagination to fetch more than 20 results (up to 60).

Error Handling: Robust error handling for API requests and JSON parsing.

Prerequisites
C++ Compiler: Ensure you have a C++ compiler installed (e.g., g++).

Boost Library: The application uses the Boost Property Tree library for JSON parsing. Install Boost if you don't have it already.

cURL: The application uses cURL for making HTTP requests to the Google Places API.

Google Places API Key: You need a valid API key from Google Cloud Platform to use the Google Places API.

Installation
Clone the Repository:

bash
Copy
git clone https://github.com/yourusername/restaurant-finder.git
cd restaurant-finder
Install Dependencies:

Boost: Install Boost using your package manager. For example, on Ubuntu:

bash
Copy
sudo apt-get install libboost-all-dev
cURL: Install cURL if not already installed:

bash
Copy
sudo apt-get install libcurl4-openssl-dev
Set Up API Key:

Create a .env file in the root directory of the project.

Add your Google Places API key to the .env file:

Copy
API_KEY=your_api_key_here
Compile the Code:

bash
Copy
g++ -o restaurant_finder main.cpp -lcurl -lboost_system -lboost_property_tree
